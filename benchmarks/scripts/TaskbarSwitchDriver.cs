using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Automation;

class TaskbarSwitchDriver {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] static extern void mouse_event(uint f,uint dx,uint dy,uint data,UIntPtr extra);
 [DllImport("user32.dll")] static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,System.Text.StringBuilder s,int n);
 [StructLayout(LayoutKind.Sequential)] struct Rect {public int L,T,R,B;}
 static string ClassName(IntPtr h){var s=new System.Text.StringBuilder(256);GetClassName(h,s,256);return s.ToString();}
 static AutomationElement Target(IntPtr bar){
  var root=AutomationElement.FromHandle(bar);
  var matches=root.FindAll(TreeScope.Descendants,new AndCondition(new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.Button),new PropertyCondition(AutomationElement.IsOffscreenProperty,false)));
  AutomationElement found=null;int count=0;
  foreach(AutomationElement e in matches){
   string name=e.Current.Name;
   if(name.IndexOf("ChatGPT",StringComparison.OrdinalIgnoreCase)>=0){Console.WriteLine("Taskbar candidate: "+name+" "+e.Current.BoundingRectangle);found=e;count++;}
  }
  if(count!=1)throw new Exception("Expected exactly one visible ChatGPT taskbar button; found "+count);
  return found;
 }
 [STAThread] static int Main(string[] args){
  Console.OutputEncoding=new System.Text.UTF8Encoding(false);
  SetProcessDPIAware();
  try{
   var bar=FindWindow("Shell_TrayWnd",null);if(bar==IntPtr.Zero)throw new Exception("Primary taskbar unavailable");
   var button=Target(bar);
   if(args.Length==0 || args[0]=="--probe"){Console.WriteLine("Probe only; no clicks sent.");return 0;}
   if(args.Length!=3 || args[0]!="--run")throw new Exception("Usage: --run output-directory target-window-handle");
   string output=Path.GetFullPath(args[1]);if(!Directory.Exists(output))throw new Exception("Output directory must exist");
   var window=new IntPtr(long.Parse(args[2],CultureInfo.InvariantCulture));
   var stop=Path.Combine(output,"stop-clicks.flag");
   object shell=Activator.CreateInstance(Type.GetTypeFromProgID("Shell.Application"));
   bool minimized=false;
   using(var events=new StreamWriter(Path.Combine(output,"click-events.csv"),false)){
    events.AutoFlush=true;events.WriteLine("utc,elapsed_s,click,interval_s,minimized_before,minimized_after,foreground_hwnd,foreground_class,target_foreground");
    try{
     shell.GetType().InvokeMember("MinimizeAll",BindingFlags.InvokeMethod,null,shell,null);minimized=true;
     Thread.Sleep(1500);
     var clock=Stopwatch.StartNew();double due=4.0,last=0;int clicks=0;
     File.WriteAllText(Path.Combine(output,"driver-ready.flag"),DateTime.UtcNow.ToString("o"));
     while(!File.Exists(stop) && clock.Elapsed.TotalMinutes<8){
      double now=clock.Elapsed.TotalSeconds;
      if(now<due){Thread.Sleep(Math.Min(50,Math.Max(1,(int)((due-now)*1000))));continue;}
      Rect r;if(!GetWindowRect(bar,out r))throw new Exception("Taskbar disappeared");
      var box=button.Current.BoundingRectangle;
      int x=(int)(box.Left+box.Width/2),y=(int)(box.Top+box.Height/2);
      if(box.IsEmpty || x<r.L || x>=r.R || y<r.T || y>=r.B)throw new Exception("Taskbar button layout changed");
      bool before=IsIconic(window);
      SetCursorPos(x,y);mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);
      // Keep the pointer on the icon: foreground selection has its own refresh
      // allowance and is intentionally tested together with hover protection.
      double clicked=clock.Elapsed.TotalSeconds;clicks++;
      Thread.Sleep(500);
      bool after=IsIconic(window);var foreground=GetForegroundWindow();
      events.WriteLine(DateTime.UtcNow.ToString("o")+","+clicked.ToString("F6",CultureInfo.InvariantCulture)+","+clicks+","+(last==0?"":(clicked-last).ToString("F6",CultureInfo.InvariantCulture))+","+before+","+after+","+foreground.ToInt64()+","+ClassName(foreground)+","+(foreground==window));
      if(before==after)throw new Exception("Click did not toggle the target's minimized state; refusing to record invalid workload");
      last=clicked;due+=4;
      if(clock.Elapsed.TotalSeconds-due>1)throw new Exception("Click cadence missed by more than one second");
     }
     File.WriteAllText(Path.Combine(output,"driver-complete.txt"),"clicks="+clicks);
    }finally{
     if(minimized)shell.GetType().InvokeMember("UndoMinimizeALL",BindingFlags.InvokeMethod,null,shell,null);
     Marshal.FinalReleaseComObject(shell);
    }
   }
   return 0;
  }catch(Exception e){Console.Error.WriteLine(e);if(args.Length>=2 && args[0]=="--run")File.WriteAllText(Path.Combine(args[1],"driver-error.txt"),e.Message);return 1;}
 }
}
