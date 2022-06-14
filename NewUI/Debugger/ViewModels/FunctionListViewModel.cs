﻿using Avalonia;
using Avalonia.Collections;
using Avalonia.Controls;
using Avalonia.Controls.Selection;
using Avalonia.Media;
using DataBoxControl;
using Dock.Model.ReactiveUI.Controls;
using Mesen.Config;
using Mesen.Debugger.Labels;
using Mesen.Debugger.Utilities;
using Mesen.Debugger.Windows;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.ViewModels;
using ReactiveUI.Fody.Helpers;
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;

namespace Mesen.Debugger.ViewModels
{
	public class FunctionListViewModel : ViewModelBase
	{
		[Reactive] public MesenList<FunctionViewModel> Functions { get; private set; } = new();
		[Reactive] public SelectionModel<FunctionViewModel?> Selection { get; set; } = new() { SingleSelect = false };
		[Reactive] public SortState SortState { get; set; } = new();

		public CpuType CpuType { get; }
		public DisassemblyViewModel Disassembly { get; }

		[Obsolete("For designer only")]
		public FunctionListViewModel() : this(CpuType.Snes, new()) { }

		public FunctionListViewModel(CpuType cpuType, DisassemblyViewModel disassembly)
		{
			CpuType = cpuType;
			Disassembly = disassembly;

			SortState.SetColumnSort("AbsAddr", ListSortDirection.Ascending, true);
		}

		public void Sort(object? param)
		{
			UpdateFunctionList();
		}

		public void UpdateFunctionList()
		{
			int selection = Selection.SelectedIndex;

			MemoryType prgMemType = CpuType.GetPrgRomMemoryType();
			List<FunctionViewModel> sortedFunctions = DebugApi.GetCdlFunctions(CpuType.GetPrgRomMemoryType()).Select(f => new FunctionViewModel(new AddressInfo() { Address = (int)f, Type = prgMemType }, CpuType)).ToList();

			Dictionary<string, Func<FunctionViewModel, FunctionViewModel, int>> comparers = new() {
				{ "Label", (a, b) => string.Compare(a.LabelName, b.LabelName, StringComparison.OrdinalIgnoreCase) },
				{ "RelAddr", (a, b) => a.RelAddress.CompareTo(b.RelAddress) },
				{ "AbsAddr", (a, b) => a.AbsAddress.CompareTo(b.AbsAddress) },
			};

			sortedFunctions.Sort((a, b) => {
				foreach((string column, ListSortDirection order) in SortState.SortOrder) {
					int result = comparers[column](a, b);
					if(result != 0) {
						return result * (order == ListSortDirection.Ascending ? 1 : -1);
					}
				}
				return a.AbsAddress.CompareTo(b.AbsAddress);
			});

			Functions.Replace(sortedFunctions);

			if(selection >= 0) {
				if(selection < Functions.Count) {
					Selection.SelectedIndex = selection;
				} else {
					Selection.SelectedIndex = Functions.Count - 1;
				}
			}
		}

		public void InitContextMenu(Control parent)
		{
			DebugShortcutManager.CreateContextMenu(parent, new object[] {
				new ContextMenuAction() {
					ActionType = ActionType.EditLabel,
					Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.FunctionList_EditLabel),
					IsEnabled = () => Selection.SelectedItems.Count == 1,
					OnClick = () => {
						if(Selection.SelectedItem is FunctionViewModel vm) {
							LabelEditWindow.EditLabel(CpuType, parent, vm.Label ?? new CodeLabel(vm.FuncAddr));
						}
					}
				},

				new ContextMenuSeparator(),

				new ContextMenuAction() {
					ActionType = ActionType.ToggleBreakpoint,
					Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.FunctionList_ToggleBreakpoint),
					IsEnabled = () => Selection.SelectedItems.Count == 1,
					OnClick = () => {
						if(Selection.SelectedItem is FunctionViewModel vm) {
							BreakpointManager.ToggleBreakpoint(vm.FuncAddr, CpuType);
						}
					}
				},

				new ContextMenuSeparator(),

				new ContextMenuAction() {
					ActionType = ActionType.GoToLocation,
					Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.FunctionList_GoToLocation),
					IsEnabled = () => Selection.SelectedItems.Count == 1 && Selection.SelectedItem is FunctionViewModel vm && vm.RelAddress >= 0,
					OnClick = () => {
						if(Selection.SelectedItem is FunctionViewModel vm) {
							if(vm.RelAddress >= 0) {
								Disassembly.SetSelectedRow(vm.RelAddress, true);
							}
						}
					}
				},

				new ContextMenuAction() {
					ActionType = ActionType.ViewInMemoryViewer,
					Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.FunctionList_ViewInMemoryViewer),
					IsEnabled = () => Selection.SelectedItems.Count == 1,
					OnClick = () => {
						if(Selection.SelectedItem is FunctionViewModel vm) {
							AddressInfo addr = new AddressInfo() { Address = vm.RelAddress, Type = CpuType.ToMemoryType() };
							if(addr.Address < 0) {
								addr = vm.FuncAddr;
							}
							MemoryToolsWindow.ShowInMemoryTools(addr.Type, addr.Address);
						}
					}
				},
			});
		}

		public class FunctionViewModel : INotifyPropertyChanged
		{
			private string _format;

			public AddressInfo FuncAddr { get; private set; }
			public CpuType _cpuType;
			
			public string AbsAddressDisplay { get; }
			public int AbsAddress => FuncAddr.Address;
			public int RelAddress { get; private set; }
			public string RelAddressDisplay => RelAddress >= 0 ? ("$" + RelAddress.ToString(_format)) : "<unavailable>";
			public object RowBrush => RelAddress >= 0 ? AvaloniaProperty.UnsetValue : Brushes.Gray;
			public FontStyle RowStyle => RelAddress >= 0 ? FontStyle.Normal : FontStyle.Italic;

			public CodeLabel? Label => LabelManager.GetLabel(FuncAddr);
			public string LabelName => Label?.Label ?? "<no label>";

			public event PropertyChangedEventHandler? PropertyChanged;

			public void Refresh()
			{
				int addr = DebugApi.GetRelativeAddress(FuncAddr, _cpuType).Address;
				if(addr != RelAddress) {
					RelAddress = addr;
					PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(RowBrush)));
					PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(RowStyle)));
					PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(RelAddressDisplay)));
				}

				PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(LabelName)));
			}

			public FunctionViewModel(AddressInfo funcAddr, CpuType cpuType)
			{
				FuncAddr = funcAddr;
				_cpuType = cpuType;
				RelAddress = DebugApi.GetRelativeAddress(FuncAddr, _cpuType).Address;
				_format = "X" + cpuType.GetAddressSize();

				AbsAddressDisplay = "$" + FuncAddr.Address.ToString(_format);
			}
		}
	}
}
