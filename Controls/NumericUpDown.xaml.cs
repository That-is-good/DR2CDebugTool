using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace DR2CDebugTool.Controls
{
    public partial class NumericUpDown : UserControl
    {
        #region 依赖属性定义

        public static readonly DependencyProperty ValueProperty =
            DependencyProperty.Register("Value", typeof(double), typeof(NumericUpDown),
                new FrameworkPropertyMetadata(0d, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault, OnValueChanged));

        public double Value
        {
            get => (double)GetValue(ValueProperty);
            set => SetValue(ValueProperty, value);
        }

        public static readonly DependencyProperty MinimumProperty =
            DependencyProperty.Register("Minimum", typeof(double), typeof(NumericUpDown), new PropertyMetadata(double.MinValue));

        public double Minimum
        {
            get => (double)GetValue(MinimumProperty);
            set => SetValue(MinimumProperty, value);
        }

        public static readonly DependencyProperty MaximumProperty =
            DependencyProperty.Register("Maximum", typeof(double), typeof(NumericUpDown), new PropertyMetadata(double.MaxValue));

        public double Maximum
        {
            get => (double)GetValue(MaximumProperty);
            set => SetValue(MaximumProperty, value);
        }

        public static readonly DependencyProperty StepProperty =
            DependencyProperty.Register("Step", typeof(double), typeof(NumericUpDown), new PropertyMetadata(1d));

        public double Step
        {
            get => (double)GetValue(StepProperty);
            set => SetValue(StepProperty, value);
        }

        // 辅助属性，便于直接操作文本框内容
        public string Text
        {
            get => ValueTextBox.Text;
            set => ValueTextBox.Text = value;
        }

        #endregion

        #region 路由事件（仅由用户交互触发）

        public static readonly RoutedEvent ValueChangedEvent =
            EventManager.RegisterRoutedEvent("ValueChanged", RoutingStrategy.Bubble,
                typeof(RoutedPropertyChangedEventHandler<double>), typeof(NumericUpDown));

        public event RoutedPropertyChangedEventHandler<double> ValueChanged
        {
            add => AddHandler(ValueChangedEvent, value);
            remove => RemoveHandler(ValueChangedEvent, value);
        }

        #endregion

        public NumericUpDown()
        {
            InitializeComponent();
            // 确保初始显示正确（尤其值为0时显示"0"）
            ValueTextBox.Text = Value.ToString(CultureInfo.InvariantCulture);
        }

        // 依赖属性变更回调：仅同步 UI 文本框，绝不触发事件
        private static void OnValueChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is not NumericUpDown control) return;

            double newValue = (double)e.NewValue;
            // 修复负零显示问题（-0 → 0）
            if (Math.Abs(newValue) < 1e-12)
                newValue = 0;

            control.ValueTextBox.Text = newValue.ToString(CultureInfo.InvariantCulture);
            // 注意：此处不再触发 ValueChanged 事件
        }

        // ---- 用户交互入口（所有用户操作均通过此方法提交） ----
        private void CommitUIChange(double newValue)
        {
            // 钳制范围
            newValue = Math.Clamp(newValue, Minimum, Maximum);
            double oldValue = Value;

            // 只有值真正改变时才更新并触发事件
            if (Math.Abs(newValue - oldValue) > 0.000001)
            {
                // 更新依赖属性（会触发 OnValueChanged 更新 UI，但不会触发事件）
                Value = newValue;

                // 手动激发路由事件，传递正确的旧值和新值
                var args = new RoutedPropertyChangedEventArgs<double>(oldValue, newValue, ValueChangedEvent);
                RaiseEvent(args);
            }
            else
            {
                // 值未变化，但输入可能无效（如格式错误），恢复显示当前值
                ValueTextBox.Text = Value.ToString(CultureInfo.InvariantCulture);
            }
        }

        // ---- 控件事件处理 ----
        private void Increase_Click(object sender, RoutedEventArgs e) => CommitUIChange(Value + Step);
        private void Decrease_Click(object sender, RoutedEventArgs e) => CommitUIChange(Value - Step);

        private void ValueTextBox_LostFocus(object sender, RoutedEventArgs e)
        {
            if (double.TryParse(ValueTextBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double result))
                CommitUIChange(result);
            else
                ValueTextBox.Text = Value.ToString(CultureInfo.InvariantCulture);
        }

        private void ValueTextBox_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                ValueTextBox_LostFocus(sender, e);
                Keyboard.ClearFocus(); // 移除焦点，触发视觉确认
            }
        }

        // 方向键支持（上/下）
        protected override void OnPreviewKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Up)
            {
                Increase_Click(this, e);
                e.Handled = true;
            }
            else if (e.Key == Key.Down)
            {
                Decrease_Click(this, e);
                e.Handled = true;
            }
            base.OnPreviewKeyDown(e);
        }
    }
}