#include "stdafx.h"
#include "PCE/Debugger/PceVdcTools.h"
#include "PCE/PceConsole.h"
#include "PCE/PceConstants.h"
#include "PCE/PceDefaultVideoFilter.h"
#include "PCE/PceTypes.h"
#include "Debugger/DebugTypes.h"
#include "Debugger/MemoryDumper.h"
#include "Shared/SettingTypes.h"

PceVdcTools::PceVdcTools(Debugger* debugger, Emulator *emu, PceConsole* console) : PpuTools(debugger, emu)
{
	_console = console;
}

FrameInfo PceVdcTools::GetTilemapSize(GetTilemapOptions options, BaseState& baseState)
{
	PceVideoState& state = (PceVideoState&)baseState;
	if(options.Layer == 0) {
		return { (uint32_t)state.Vdc.ColumnCount * 8, (uint32_t)state.Vdc.RowCount * 8 };
	} else {
		return { (uint32_t)state.Vdc2.ColumnCount * 8, (uint32_t)state.Vdc2.RowCount * 8 };
	}
}

DebugTilemapTileInfo PceVdcTools::GetTilemapTileInfo(uint32_t x, uint32_t y, uint8_t* vram, GetTilemapOptions options, BaseState& baseState)
{
	DebugTilemapTileInfo result;

	FrameInfo size = GetTilemapSize(options, baseState);
	if(x >= size.Width || y >= size.Height) {
		return result;
	}

	PceVdcState& state = options.Layer == 0 ? ((PceVideoState&)baseState).Vdc : ((PceVideoState&)baseState).Vdc2;

	uint32_t row = y / 8;
	uint32_t column = x / 8;

	uint16_t entryAddr = (row * state.ColumnCount + column) * 2;
	uint16_t batEntry = vram[entryAddr] | (vram[entryAddr + 1] << 8);

	result.Height = 8;
	result.Width = 8;
	result.PaletteIndex = batEntry >> 12;
	result.TileIndex = (batEntry & 0xFFF);
	result.TileMapAddress = entryAddr;

	result.TileAddress = result.TileIndex * 32;
	result.PaletteAddress = result.PaletteIndex * 16;

	result.Row = row;
	result.Column = column;

	return result;
}

DebugTilemapInfo PceVdcTools::GetTilemap(GetTilemapOptions options, BaseState& baseState, uint8_t* vram, uint32_t* palette, uint32_t* outBuffer)
{
	PceVdcState& state = options.Layer == 0 ? ((PceVideoState&)baseState).Vdc : ((PceVideoState&)baseState).Vdc2;

	DebugTilemapInfo result = {};
	result.Bpp = 4;
	result.Format = TileFormat::Bpp4;
	result.TileWidth = 8;
	result.TileHeight = 8;
	result.ColumnCount = state.ColumnCount;
	result.RowCount = state.RowCount;
	result.TilemapAddress = 0;
	result.TilesetAddress = 0;
	result.ScrollX = state.HvReg.BgScrollX;
	result.ScrollY = state.HvReg.BgScrollY;
	result.ScrollWidth = (state.HvLatch.HorizDisplayWidth + 1) * 8;
	result.ScrollHeight = std::min<uint32_t>(242, state.HvLatch.VertDisplayWidth);

	for(uint8_t row = 0; row < state.RowCount; row++) {
		for(uint8_t column = 0; column < state.ColumnCount; column++) {
			uint16_t entryAddr = (row * state.ColumnCount + column) * 2;
			uint16_t batEntry = vram[entryAddr] | (vram[entryAddr + 1] << 8);
			uint8_t palIndex = batEntry >> 12;
			uint16_t tileIndex = (batEntry & 0xFFF);

			for(int y = 0; y < 8; y++) {
				uint16_t tileAddr = tileIndex * 32 + y * 2;
				for(int x = 0; x < 8; x++) {
					uint8_t color = GetTilePixelColor(vram, 0xFFFF, tileAddr, 7 - x, TileFormat::Bpp4);
					uint16_t palAddr = color == 0 ? 0 : (palIndex * 16 + color);
					uint32_t outPos = (row * 8 + y) * state.ColumnCount * 8 + column * 8 + x;
					outBuffer[outPos] = palette[palAddr];
				}
			}
		}
	}

	return result;
}

void PceVdcTools::GetSpritePreview(GetSpritePreviewOptions options, BaseState& baseState, uint8_t* vram, uint8_t* oamRam, uint32_t* palette, uint32_t* outBuffer)
{
	PceVdcState& state = ((PceVideoState&)baseState).Vdc;

	uint32_t screenWidth = std::min<uint32_t>(PceConstants::MaxScreenWidth, (state.HvLatch.HorizDisplayWidth + 1) * 8);
	std::fill(outBuffer, outBuffer + 1024*1024, 0xFF333333);
	for(int i = 64; i < state.HvLatch.VertDisplayWidth + 64; i++) {
		std::fill(outBuffer + i * 1024 + 32, outBuffer + i * 1024 + 32 + screenWidth, 0xFF666666);
	}

	int spriteCount = _console->IsSuperGrafx() ? 128 : 64;

	DebugSpriteInfo sprite;
	for(int i = spriteCount - 1; i >= 0; i--) {
		GetSpriteInfo(sprite, i, options, vram, oamRam, palette);

		for(int y = 0; y < sprite.Height; y++) {
			if(sprite.Y + y >= 1024) {
				break;
			}

			for(int x = 0; x < sprite.Width; x++) {
				if(sprite.X + x >= 1024) {
					break;
				}

				uint32_t color = sprite.SpritePreview[y * sprite.Width + x];
				if(color != 0) {
					outBuffer[((sprite.Y + y) * 1024) + sprite.X + x] = color;
				}
			}
		}
	}
}

void PceVdcTools::GetSpriteInfo(DebugSpriteInfo& sprite, uint16_t spriteIndex, GetSpritePreviewOptions& options, uint8_t* vram, uint8_t* oamRam, uint32_t* palette)
{
	uint16_t addr = (spriteIndex * 8);

	uint16_t spriteY = (oamRam[addr] | (oamRam[addr+1] << 8)) & 0x3FF;
	uint16_t spriteX = (oamRam[addr + 2] | (oamRam[addr + 3] << 8)) & 0x3FF;
	uint16_t patternCode = (oamRam[addr + 4] | (oamRam[addr + 5] << 8)) & 0x7FF;
	uint16_t tileIndex = patternCode >> 1;
	uint16_t flags = (oamRam[addr + 6] | (oamRam[addr + 7] << 8));
	
	uint8_t width = (flags & 0x100) ? 32 : 16;
	uint8_t height;
	switch((flags >> 12) & 0x03) {
		default:
		case 0: height = 16; break;
		case 1: height = 32; break;
		
		case 2:
		case 3:
			height = 64;
			break;
	}

	bool visible = (
		((spriteX + width) > 32 && (spriteX < 256 + 32)) ||
		((spriteY + height) > 64 && (spriteY < 242 + 64))
	);

	sprite.Bpp = 4;
	sprite.Format = TileFormat::PceSpriteBpp4;
	sprite.SpriteIndex = spriteIndex;
	sprite.UseExtendedVram = spriteIndex >= 64;
	sprite.X = spriteX;
	sprite.Y = spriteY;
	sprite.RawX = spriteX;
	sprite.RawY = spriteY;
	sprite.Height = height;
	sprite.Width = width;
	sprite.TileIndex = tileIndex;
	sprite.Palette = (flags & 0x0F);
	sprite.PaletteAddress = (sprite.Palette + 16) * 16;
	sprite.Priority = (flags & 0x80) ? DebugSpritePriority::Foreground : DebugSpritePriority::Background;
	sprite.HorizontalMirror = (flags & 0x800) != 0;
	sprite.VerticalMirror = (flags & 0x8000) != 0;
	sprite.Visible = visible;
	sprite.UseSecondTable = NullableBoolean::Undefined;

	if(sprite.UseExtendedVram) {
		//Sprite for VDC2, use VRAM from VDC2
		vram += 0x10000;
	}

	if(width == 32) {
		tileIndex &= ~0x01;
	}
	if(height == 32) {
		tileIndex &= ~0x02;
	} else if(height == 64) {
		tileIndex &= ~0x06;
	}
	
	sprite.TileAddress = tileIndex * 64 * 2;

	uint8_t yOffset;
	int rowOffset;

	for(int y = 0; y < sprite.Height; y++) {
		if(sprite.VerticalMirror) {
			yOffset = (sprite.Height - y - 1) & 0x0F;
			rowOffset = (sprite.Height - y - 1) >> 4;
		} else {
			yOffset = y & 0x0F;
			rowOffset = y >> 4;
		}

		uint16_t spriteTileY = tileIndex | (rowOffset << 1);

		for(int x = 0; x < sprite.Width; x++) {
			uint32_t outOffset = y * sprite.Width + x;

			uint8_t xOffset;
			int columnOffset;
			if(sprite.HorizontalMirror) {
				xOffset = (sprite.Width - x - 1) & 0x0F;
				columnOffset = (sprite.Width - x - 1) >> 4;
			} else {
				xOffset = x & 0x0F;
				columnOffset = x >> 4;
			}

			uint16_t spriteTile = spriteTileY | columnOffset;
			uint16_t tileStart = spriteTile * 64;

			uint16_t pixelStart = tileStart + yOffset;
			uint8_t shift = 15 - xOffset;

			uint8_t color;
			if(pixelStart >= 0x8000) {
				//Display specific pattern when open bus, to make problem obvious in sprite viewer
				color = xOffset;
			} else {
				color = GetTilePixelColor(vram, 0xFFFF, pixelStart * 2, shift, TileFormat::PceSpriteBpp4);
			}			

			if(color != 0) {
				sprite.SpritePreview[outOffset] = palette[color + sprite.Palette * 16 + 16*16];
			} else {
				sprite.SpritePreview[outOffset] = 0;
			}
		}
	}
}

void PceVdcTools::GetSpriteList(GetSpritePreviewOptions options, BaseState& baseState, uint8_t* vram, uint8_t* oamRam, uint32_t* palette, DebugSpriteInfo outBuffer[])
{
	for(int i = 0, len = _console->IsSuperGrafx() ? 128 : 64; i < len; i++) {
		outBuffer[i].Init();
		GetSpriteInfo(outBuffer[i], i, options, vram, oamRam, palette);
	}
}

DebugSpritePreviewInfo PceVdcTools::GetSpritePreviewInfo(GetSpritePreviewOptions options, BaseState& state)
{
	DebugSpritePreviewInfo info = {};
	info.Height = 1024;
	info.Width = 1024; 
	info.SpriteCount = _console->IsSuperGrafx() ? 128 : 64;
	info.CoordOffsetX = 0;
	info.CoordOffsetY = 0;
	return info;
}

DebugPaletteInfo PceVdcTools::GetPaletteInfo()
{
	DebugPaletteInfo info = {};
	info.RawFormat = RawPaletteFormat::Rgb333;
	info.ColorsPerPalette = 16;
	info.BgColorCount = 16 * 16;
	info.SpriteColorCount = 16 * 16;
	info.ColorCount = info.BgColorCount + info.SpriteColorCount;

	uint32_t* palette = _emu->GetSettings()->GetPcEngineConfig().Palette;

	uint8_t* pal = _debugger->GetMemoryDumper()->GetMemoryBuffer(MemoryType::PcePaletteRam);
	for(int i = 0; i < 512; i++) {
		info.RawPalette[i] = pal[i * 2] | (pal[i * 2 + 1] << 8);
		info.RgbPalette[i] = palette[info.RawPalette[i]];
	}

	return info;
}