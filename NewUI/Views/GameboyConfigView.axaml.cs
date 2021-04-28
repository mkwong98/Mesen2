using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Mesen.Utilities;
using Mesen.GUI.Config;
using Mesen.Debugger.Controls;
using Mesen.ViewModels;
using Mesen.Windows;
using System;
using Avalonia.Media;
using System.Threading.Tasks;
using Avalonia.Interactivity;

namespace Mesen.Views
{
	public class GameboyConfigView : UserControl
	{
		public GameboyConfigView()
		{
			InitializeComponent();
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		private async Task<Color> SelectColor(Color color)
		{
			ColorPickerViewModel model = new ColorPickerViewModel() { Color = color };
			ColorPickerWindow wnd = new ColorPickerWindow() {
				DataContext = model
			};
			wnd.WindowStartupLocation = WindowStartupLocation.CenterOwner;

			bool success = await wnd.ShowDialog<bool>(VisualRoot as Window);
			if(success) {
				return model.Color;
			}
			return color;
		}

		private async void BgColor_OnClick(object sender, PaletteSelector.ColorClickEventArgs e)
		{
			Color color = await SelectColor(e.Color);
			UInt32[] colors = (UInt32[])(DataContext as GameboyConfigViewModel).Config.BgColors.Clone();
			colors[e.ColorIndex] = color.ToUint32();
			(DataContext as GameboyConfigViewModel).Config.BgColors = colors;
		}

		private async void Obj0Color_OnClick(object sender, PaletteSelector.ColorClickEventArgs e)
		{
			Color color = await SelectColor(e.Color);
			UInt32[] colors = (UInt32[])(DataContext as GameboyConfigViewModel).Config.Obj0Colors.Clone();
			colors[e.ColorIndex] = color.ToUint32();
			(DataContext as GameboyConfigViewModel).Config.Obj0Colors = colors;
		}

		private async void Obj1Color_OnClick(object sender, PaletteSelector.ColorClickEventArgs e)
		{
			Color color = await SelectColor(e.Color);
			UInt32[] colors = (UInt32[])(DataContext as GameboyConfigViewModel).Config.Obj1Colors.Clone();
			colors[e.ColorIndex] = color.ToUint32();
			(DataContext as GameboyConfigViewModel).Config.Obj1Colors = colors;
		}

		private void btnSelectPreset_OnClick(object sender, RoutedEventArgs e)
		{
			((Button)sender).ContextMenu.Open();
		}

		private void mnuGrayscalePreset_Click(object sender, RoutedEventArgs e)
		{
			SetPalette(Color.FromArgb(255, 232, 232, 232), Color.FromArgb(255, 160, 160, 160), Color.FromArgb(255, 88, 88, 88), Color.FromArgb(255, 16, 16, 16));
		}

		private void mnuGrayscaleHighContrastPreset_Click(object sender, RoutedEventArgs e)
		{
			SetPalette(Color.FromArgb(255, 255, 255, 255), Color.FromArgb(255, 176, 176, 176), Color.FromArgb(255, 104, 104, 104), Color.FromArgb(255, 0, 0, 0));
		}

		private void mnuGreenPreset_Click(object sender, RoutedEventArgs e)
		{
			SetPalette(Color.FromArgb(255, 224, 248, 208), Color.FromArgb(255, 136, 192, 112), Color.FromArgb(255, 52, 104, 86), Color.FromArgb(255, 8, 24, 32));
		}

		private void mnuBrownPreset_Click(object sender, RoutedEventArgs e)
		{
			SetPalette(Color.FromArgb(255, 248, 224, 136), Color.FromArgb(255, 216, 176, 88), Color.FromArgb(255, 152, 120, 56), Color.FromArgb(255, 72, 56, 24));
		}

		private void SetPalette(Color color0, Color color1, Color color2, Color color3)
		{
			GameboyConfigViewModel model = this.DataContext as GameboyConfigViewModel;
			model.Config.BgColors = new UInt32[] { color0.ToUint32(), color1.ToUint32(), color2.ToUint32(), color3.ToUint32() };
			model.Config.Obj0Colors = new UInt32[] { color0.ToUint32(), color1.ToUint32(), color2.ToUint32(), color3.ToUint32() };
			model.Config.Obj1Colors = new UInt32[] { color0.ToUint32(), color1.ToUint32(), color2.ToUint32(), color3.ToUint32() };
		}
	}
}