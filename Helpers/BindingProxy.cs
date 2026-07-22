using System.Windows;

namespace DR2CDebugTool.Helpers
{
    /// <summary>
    /// 用于DataGrid列头绑定的代理。
    /// DataGridColumn不在VisualTree中，无法直接绑定DataContext。
    /// 这个代理通过Freezable传递DataContext。
    /// </summary>
    public class BindingProxy : Freezable
    {
        protected override Freezable CreateInstanceCore() => new BindingProxy();

        public object Data
        {
            get => GetValue(DataProperty);
            set => SetValue(DataProperty, value);
        }

        public static readonly DependencyProperty DataProperty =
            DependencyProperty.Register(nameof(Data), typeof(object), typeof(BindingProxy), new PropertyMetadata(null));
    }
}