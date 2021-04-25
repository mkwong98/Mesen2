#include "stdafx.h"
#include "NES/Mappers/VsSystem/VsControlManager.h"
#include "NES/Mappers/VsSystem/VsSystem.h"
#include "NES/Mappers/VsSystem/VsInputButtons.h"
#include "NES/Input/VsZapper.h"
#include "NES/Input/NesController.h"
#include "NES/NesConsole.h"
#include "NES/NesCpu.h"
#include "Shared/NotificationManager.h"

VsControlManager::VsControlManager(NesConsole* console) : NesControlManager(console)
{
	_input.reset(new VsInputButtons(_emu, true)); //TODO

	if(_console->IsVsMainConsole()) {
		_emu->GetNotificationManager()->RegisterNotificationListener(_input);
	} else 	{
		//Remove SystemActionManager (reset/power buttons) from sub console
		_systemDevices.clear();
	}

	AddSystemControlDevice(_input);
}

VsControlManager::~VsControlManager()
{
	UnregisterInputProvider(this);
}

ControllerType VsControlManager::GetControllerType(uint8_t port)
{
	ControllerType type = NesControlManager::GetControllerType(port);
	if(type == ControllerType::NesZapper) {
		type = ControllerType::VsZapper;
	}
	return type;
}

void VsControlManager::Reset(bool softReset)
{
	NesControlManager::Reset(softReset);
	_protectionCounter = 0;

	//Unsure about this, needed for VS Wrecking Crew
	UpdateSlaveMasterBit(_console->IsVsMainConsole() ? 0x00 : 0x02);

	_vsSystemType = _console->GetMapper()->GetRomInfo().VsType;

	if(!softReset && !_console->IsVsMainConsole()) {
		UnregisterInputProvider(this);
		RegisterInputProvider(this);
	}
}

void VsControlManager::Serialize(Serializer& s)
{
	NesControlManager::Serialize(s);
	s.Stream(_prgChrSelectBit, _protectionCounter, _refreshState);
}

void VsControlManager::GetMemoryRanges(MemoryRanges &ranges)
{
	NesControlManager::GetMemoryRanges(ranges);
	ranges.AddHandler(MemoryOperation::Read, 0x4020, 0x5FFF);
	ranges.AddHandler(MemoryOperation::Write, 0x4020, 0x5FFF);
}

uint8_t VsControlManager::GetPrgChrSelectBit()
{
	return _prgChrSelectBit;
}

void VsControlManager::RemapControllerButtons()
{
	shared_ptr<NesController> controllers[2];
	controllers[0] = std::dynamic_pointer_cast<NesController>(GetControlDevice(0));
	controllers[1] = std::dynamic_pointer_cast<NesController>(GetControlDevice(1));

	if(!controllers[0] || !controllers[1]) {
		return;
	}

	GameInputType inputType = _console->GetMapper()->GetRomInfo().InputType;
	if(inputType == GameInputType::VsSystemSwapped) {
		//Swap controllers 1 & 2
		ControlDeviceState port1State = controllers[0]->GetRawState();
		ControlDeviceState port2State = controllers[1]->GetRawState();
		controllers[0]->SetRawState(port2State);
		controllers[1]->SetRawState(port1State);

		//But don't swap the start/select buttons
		BaseControlDevice::SwapButtons(controllers[0], NesController::Buttons::Start, controllers[1], NesController::Buttons::Start);
		BaseControlDevice::SwapButtons(controllers[0], NesController::Buttons::Select, controllers[1], NesController::Buttons::Select);
	} else if(inputType == GameInputType::VsSystemSwapAB) {
		//Swap buttons P1 A & P2 B (Pinball (Japan))
		BaseControlDevice::SwapButtons(controllers[0], NesController::Buttons::B, controllers[1], NesController::Buttons::A);
	}

	//Swap Start/Select for all configurations (makes it more intuitive)
	BaseControlDevice::SwapButtons(controllers[0], NesController::Buttons::Start, controllers[0], NesController::Buttons::Select);
	BaseControlDevice::SwapButtons(controllers[1], NesController::Buttons::Start, controllers[1], NesController::Buttons::Select);

	if(_vsSystemType == VsSystemType::RaidOnBungelingBayProtection || _vsSystemType == VsSystemType::IceClimberProtection) {
		//Bit 3 of the input status must always be on
		controllers[0]->SetBit(NesController::Buttons::Start);
		controllers[1]->SetBit(NesController::Buttons::Start);
	}
}

uint8_t VsControlManager::GetOpenBusMask(uint8_t port)
{
	return 0x00;
}

uint8_t VsControlManager::ReadRam(uint16_t addr)
{
	uint8_t value = 0;

	if(!_console->IsVsMainConsole()) {
		//Copy the insert coin 3/4 + service button "2" bits from the main console to this one
		NesConsole* mainConsole = _console->GetVsMainConsole();
		VsInputButtons* mainButtons = ((VsControlManager*)mainConsole->GetControlManager().get())->_input.get();
		_input->SetBitValue(VsInputButtons::VsButtons::InsertCoin1, mainButtons->IsPressed(VsInputButtons::VsButtons::InsertCoin3));
		_input->SetBitValue(VsInputButtons::VsButtons::InsertCoin2, mainButtons->IsPressed(VsInputButtons::VsButtons::InsertCoin4));
		_input->SetBitValue(VsInputButtons::VsButtons::ServiceButton, mainButtons->IsPressed(VsInputButtons::VsButtons::ServiceButton2));
	}

	switch(addr) {
		case 0x4016: {
			uint32_t dipSwitches = _emu->GetSettings()->GetNesConfig().DipSwitches;
			if(!_console->IsVsMainConsole()) {
				dipSwitches >>= 8;
			}

			value = NesControlManager::ReadRam(addr) & 0x65;
			value |= ((dipSwitches & 0x01) ? 0x08 : 0x00);
			value |= ((dipSwitches & 0x02) ? 0x10 : 0x00);
			value |= (_console->IsVsMainConsole() ? 0x00 : 0x80);
			break;
		}

		case 0x4017: {
			value = NesControlManager::ReadRam(addr) & 0x01;

			uint32_t dipSwitches = _emu->GetSettings()->GetNesConfig().DipSwitches;
			if(!_console->IsVsMainConsole()) {
				dipSwitches >>= 8;
			}
			value |= ((dipSwitches & 0x04) ? 0x04 : 0x00);
			value |= ((dipSwitches & 0x08) ? 0x08 : 0x00);
			value |= ((dipSwitches & 0x10) ? 0x10 : 0x00);
			value |= ((dipSwitches & 0x20) ? 0x20 : 0x00);
			value |= ((dipSwitches & 0x40) ? 0x40 : 0x00);
			value |= ((dipSwitches & 0x80) ? 0x80 : 0x00);
			break;
		}

		case 0x5E00:
			_protectionCounter = 0;
			break;

		case 0x5E01:
			if(_vsSystemType == VsSystemType::TkoBoxingProtection) {
				value = _protectionData[0][_protectionCounter++ & 0x1F];
			} else if(_vsSystemType == VsSystemType::RbiBaseballProtection) {
				value = _protectionData[1][_protectionCounter++ & 0x1F];
			}
			break;

		default:
			if(_vsSystemType == VsSystemType::SuperXeviousProtection) {
				return _protectionData[2][_protectionCounter++ & 0x1F];
			}
			break;
	}

	return value;
}

void VsControlManager::WriteRam(uint16_t addr, uint8_t value)
{
	NesControlManager::WriteRam(addr, value);

	_refreshState = (value & 0x01) == 0x01;

	if(addr == 0x4016) {
		_prgChrSelectBit = (value >> 2) & 0x01;
		
		//Bit 2: DualSystem-only
		uint8_t slaveMasterBit = (value & 0x02);
		if(slaveMasterBit != _slaveMasterBit) {
			UpdateSlaveMasterBit(slaveMasterBit);
		}
	}
}

void VsControlManager::UpdateSlaveMasterBit(uint8_t slaveMasterBit)
{
	NesConsole* otherConsole = _console->GetVsMainConsole() ? _console->GetVsMainConsole() : _console->GetVsSubConsole();
	if(otherConsole) {
		VsSystem* mapper = dynamic_cast<VsSystem*>(_console->GetMapper());
		
		if(_console->IsVsMainConsole()) {
			mapper->UpdateMemoryAccess(slaveMasterBit);
		}

		if(slaveMasterBit) {
			otherConsole->GetCpu()->ClearIrqSource(IRQSource::External);
		} else {
			//When low, asserts /IRQ on the other CPU
			otherConsole->GetCpu()->SetIrqSource(IRQSource::External);
		}
	}
	_slaveMasterBit = slaveMasterBit;
}

void VsControlManager::UpdateControlDevices()
{
	if(_console->GetVsMainConsole() || _console->GetVsSubConsole()) {
		auto lock = _deviceLock.AcquireSafe();
		ClearDevices();

		//Force 4x standard controllers
		//P3 & P4 will be sent to the slave CPU - see SetInput() below.
		for(int i = 0; i < 4; i++) {
			shared_ptr<BaseControlDevice> device = CreateControllerDevice(ControllerType::NesController, i);
			if(device) {
				RegisterControlDevice(device);
			}
		}
	} else {
		NesControlManager::UpdateControlDevices();
	}
}

bool VsControlManager::SetInput(BaseControlDevice* device)
{
	uint8_t port = device->GetPort();
	NesControlManager* mainControlManager = (NesControlManager*)_console->GetVsMainConsole()->GetControlManager().get();
	if(mainControlManager && port <= 1) {
		shared_ptr<BaseControlDevice> controlDevice = mainControlManager->GetControlDevice(port + 2);
		if(controlDevice) {
			ControlDeviceState state = controlDevice->GetRawState();
			device->SetRawState(state);
		}
	}
	return true;
}