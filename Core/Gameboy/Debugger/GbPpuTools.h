#pragma once
#include "stdafx.h"
#include "Debugger/PpuTools.h"

class Debugger;
class Emulator;

class GbPpuTools : public PpuTools
{
private:
	DebugSpriteInfo GetSpriteInfo(uint16_t spriteIndex, GetSpritePreviewOptions& options, GbPpuState& state, uint8_t* oamRam);

public:
	GbPpuTools(Debugger* debugger, Emulator *emu);

	void GetTilemap(GetTilemapOptions options, BaseState& state, uint8_t* vram, uint32_t* palette, uint32_t *outBuffer) override;
	FrameInfo GetTilemapSize(GetTilemapOptions options, BaseState& state) override;

	void GetSpritePreview(GetSpritePreviewOptions options, BaseState& state, uint8_t* vram, uint8_t* oamRam, uint32_t* palette, uint32_t *outBuffer) override;
	FrameInfo GetSpritePreviewSize(GetSpritePreviewOptions options, BaseState& state) override;
	uint32_t GetSpriteList(GetSpritePreviewOptions options, BaseState& baseState, uint8_t* oamRam, DebugSpriteInfo outBuffer[]) override;
};