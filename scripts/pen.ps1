# Lápiz simulado para la prueba de la app real (HU-116): InjectSyntheticPointerInput con PT_PEN,
# así el lienzo recibe WM_POINTER como de la tableta. Posiciones en píxeles físicos de pantalla.
# Uso: . scripts/pen.ps1; Barrel x y; Tap x y; Pen x y $true (botón apretado en el aire).
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SynthPen {
    [StructLayout(LayoutKind.Explicit, Size = 152)]
    struct TypeInfo {
        [FieldOffset(0)] public int type;
        [FieldOffset(8)] public int pointerType;
        [FieldOffset(12)] public uint pointerId;
        [FieldOffset(16)] public uint frameId;
        [FieldOffset(20)] public uint pointerFlags;
        [FieldOffset(24)] public IntPtr sourceDevice;
        [FieldOffset(32)] public IntPtr hwndTarget;
        [FieldOffset(40)] public int x;
        [FieldOffset(44)] public int y;
        [FieldOffset(104)] public uint penFlags;
        [FieldOffset(108)] public uint penMask;
        [FieldOffset(112)] public uint pressure;
    }
    [DllImport("user32.dll", SetLastError = true)] static extern IntPtr CreateSyntheticPointerDevice(int type, uint maxCount, int mode);
    [DllImport("user32.dll", SetLastError = true)] static extern bool InjectSyntheticPointerInput(IntPtr device, ref TypeInfo info, uint count);
    static IntPtr dev;
    public static void Send(int x, int y, bool inContact, bool barrel, string phase) {
        if (dev == IntPtr.Zero) dev = CreateSyntheticPointerDevice(3, 1, 1);
        if (dev == IntPtr.Zero) throw new Exception("CreateSyntheticPointerDevice " + Marshal.GetLastWin32Error());
        var t = new TypeInfo();
        t.type = 3; t.pointerType = 3;
        uint f = 0x2; // INRANGE
        if (phase == "down") f |= 0x4 | 0x10 | 0x10000;
        else if (phase == "up") f |= 0x40000;
        else { f |= 0x20000; if (inContact) f |= 0x4 | 0x10; }
        t.pointerFlags = f;
        t.x = x; t.y = y;
        t.penFlags = barrel ? 1u : 0u;
        t.penMask = 1; t.pressure = inContact || phase == "down" ? 512u : 0u;
        if (!InjectSyntheticPointerInput(dev, ref t, 1)) throw new Exception("Inject " + Marshal.GetLastWin32Error());
    }
}
'@
function Pen([int]$x, [int]$y, [bool]$barrel = $false, [string]$phase = 'move', [int]$ms = 30) {
    [SynthPen]::Send($x, $y, $false, $barrel, $phase); Start-Sleep -Milliseconds $ms
}
# Un toque con la punta en (x, y).
function Tap([int]$x, [int]$y) {
    Pen $x $y; Pen $x $y $false 'down' 60; Pen $x $y $false 'up' 60; Pen $x $y
}
# Apretar y soltar el botón lateral en el aire en (x, y).
function Barrel([int]$x, [int]$y) {
    Pen $x $y; Pen $x $y $true 'move' 300; Pen $x $y $false 'move' 200
}
