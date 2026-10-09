# Íconos de Ejercicios, Cartuchera y el instalador (HU-70), con los colores del diseño "Taller
# nocturno" (docs/diseno.md). Se dibujan en vector a cada tamaño (no se escalan), así se leen
# también a 16 px, y se empaquetan en .ico con entradas PNG.
# Corre en Windows PowerShell 5.1 (GDI+ de .NET Framework):
#   powershell -File assets/icons/make-icons.ps1            # genera los .ico
#   powershell -File assets/icons/make-icons.ps1 -Preview   # además, una lámina PNG para mirarlos
param([switch]$Preview)
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class TrazosIcons
{
    static readonly Color Fondo = Color.FromArgb(0x22, 0x26, 0x2B);
    static readonly Color Borde = Color.FromArgb(0x3A, 0x40, 0x48);
    static readonly Color Hoja = Color.FromArgb(0xF5, 0xF0, 0xE6);
    static readonly Color Tinta = Color.FromArgb(0x11, 0x11, 0x11);
    static readonly Color Guia = Color.FromArgb(0x25, 0x63, 0xEB);
    static readonly Color Enfasis = Color.FromArgb(0xE0, 0x8A, 0x1E);
    static readonly Color Madera = Color.FromArgb(0xE9, 0xC8, 0x9B);
    static readonly Color Grafito = Color.FromArgb(0x3B, 0x3F, 0x45);

    static GraphicsPath Rounded(RectangleF r, float radius)
    {
        var p = new GraphicsPath();
        float d = radius * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    // Todo se dibuja en una grilla de 256 y se escala con la transformación: las formas son
    // vectoriales, y a tamaños chicos se engrosan los trazos para que no desaparezcan.
    static Bitmap Canvas(int size, out Graphics g, out float k)
    {
        var bmp = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        g = Graphics.FromImage(bmp);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.Clear(Color.Transparent);
        g.ScaleTransform(size / 256f, size / 256f);
        k = size <= 24 ? 1.8f : size <= 48 ? 1.3f : 1f; // engrosado para tamaños chicos
        return bmp;
    }

    static void Tile(Graphics g)
    {
        using (var path = Rounded(new RectangleF(8, 8, 240, 240), 48))
        using (var fill = new SolidBrush(Fondo))
        using (var pen = new Pen(Borde, 6))
        {
            g.FillPath(fill, path);
            g.DrawPath(pen, path);
        }
    }

    static void Sheet(Graphics g, RectangleF r)
    {
        using (var path = Rounded(r, 14))
        using (var fill = new SolidBrush(Hoja))
            g.FillPath(fill, path);
    }

    static void Target(Graphics g, PointF c, float k, Color color)
    {
        using (var pen = new Pen(color, 9 * k))
            g.DrawEllipse(pen, c.X - 20, c.Y - 20, 40, 40);
        using (var fill = new SolidBrush(color))
            g.FillEllipse(fill, c.X - 8 * k, c.Y - 8 * k, 16 * k, 16 * k);
    }

    // Ejercicios: la hoja con dos puntos a unir y el trazo de grafito entre ellos.
    static void Exercise(Graphics g, float k)
    {
        Sheet(g, new RectangleF(40, 40, 176, 176));
        var a = new PointF(78, 170);
        var b = new PointF(178, 86);
        using (var pen = new Pen(Tinta, 12 * k) { StartCap = LineCap.Round, EndCap = LineCap.Round })
            g.DrawBezier(pen, a, new PointF(100, 110), new PointF(150, 150), b);
        Target(g, a, k, Guia);
        Target(g, b, k, Guia);
    }

    // Un lápiz en diagonal: cuerpo ámbar, madera y punta de grafito.
    static void Pencil(Graphics g, float k, float scale, PointF offset)
    {
        var state = g.Save();
        g.TranslateTransform(offset.X, offset.Y);
        g.ScaleTransform(scale, scale);
        g.TranslateTransform(128, 128);
        g.RotateTransform(-45);
        // A lo largo de x: de la goma (-100) a la punta (+100).
        using (var eraser = new SolidBrush(Color.FromArgb(0xE8, 0x9A, 0xA0)))
            g.FillRectangle(eraser, -100, -22, 22, 44);
        using (var ferrule = new SolidBrush(Color.FromArgb(0xB8, 0xBE, 0xC6)))
            g.FillRectangle(ferrule, -80, -22, 16, 44);
        using (var body = new SolidBrush(Enfasis))
            g.FillRectangle(body, -66, -22, 110, 44);
        using (var stripe = new SolidBrush(Color.FromArgb(0xC2, 0x72, 0x10)))
            g.FillRectangle(stripe, -66, -4, 110, 8);
        using (var wood = new SolidBrush(Madera))
            g.FillPolygon(wood, new[] { new PointF(44, -22), new PointF(92, -6), new PointF(92, 6), new PointF(44, 22) });
        using (var lead = new SolidBrush(Grafito))
            g.FillPolygon(lead, new[] { new PointF(80, -10), new PointF(104, 0), new PointF(80, 10) });
        g.Restore(state);
    }

    static Bitmap Draw(string which, int size)
    {
        Graphics g; float k;
        var bmp = Canvas(size, out g, out k);
        using (g)
        {
            Tile(g);
            if (which == "ejercicios")
                Exercise(g, k);
            else if (which == "cartuchera")
                Pencil(g, k, 1f, new PointF(0, 0));
            else
            {
                Exercise(g, k);
                Pencil(g, k, 0.62f, new PointF(96, 96)); // el lápiz sobre la hoja, abajo a la derecha
            }
        }
        return bmp;
    }

    public static readonly int[] Sizes = { 16, 24, 32, 48, 64, 128, 256 };

    public static void WriteIco(string which, string path)
    {
        var pngs = new byte[Sizes.Length][];
        for (int i = 0; i < Sizes.Length; ++i)
            using (var bmp = Draw(which, Sizes[i]))
            using (var ms = new System.IO.MemoryStream())
            {
                bmp.Save(ms, ImageFormat.Png);
                pngs[i] = ms.ToArray();
            }
        using (var f = new System.IO.BinaryWriter(System.IO.File.Create(path)))
        {
            f.Write((short)0); f.Write((short)1); f.Write((short)Sizes.Length);
            int offset = 6 + 16 * Sizes.Length;
            for (int i = 0; i < Sizes.Length; ++i)
            {
                int s = Sizes[i];
                f.Write((byte)(s >= 256 ? 0 : s)); f.Write((byte)(s >= 256 ? 0 : s));
                f.Write((byte)0); f.Write((byte)0); f.Write((short)1); f.Write((short)32);
                f.Write(pngs[i].Length); f.Write(offset);
                offset += pngs[i].Length;
            }
            foreach (var png in pngs) f.Write(png);
        }
    }

    // Lámina de vista previa: los tres íconos a 256, 48, 32 y 16 sobre fondo claro y oscuro.
    public static void WritePreview(string path)
    {
        string[] names = { "ejercicios", "cartuchera", "instalador" };
        int[] sizes = { 256, 48, 32, 16 };
        using (var sheet = new Bitmap(3 * 300, 2 * 300))
        using (var g = Graphics.FromImage(sheet))
        {
            g.Clear(Color.FromArgb(0xEC, 0xEE, 0xF0));
            g.FillRectangle(new SolidBrush(Color.FromArgb(0x17, 0x19, 0x1C)), 0, 300, 900, 300);
            for (int row = 0; row < 2; ++row)
                for (int i = 0; i < 3; ++i)
                {
                    int x = i * 300 + 10, y = row * 300 + 10;
                    using (var big = Draw(names[i], 256)) g.DrawImage(big, x, y, 200, 200);
                    int sx = x + 210;
                    int sy = y;
                    for (int j = 1; j < sizes.Length; ++j)
                    {
                        using (var small = Draw(names[i], sizes[j])) g.DrawImageUnscaled(small, sx, sy);
                        sy += sizes[j] + 12;
                    }
                }
            sheet.Save(path, ImageFormat.Png);
        }
    }
}
'@
$dir = $PSScriptRoot
foreach ($name in 'ejercicios', 'cartuchera', 'instalador') {
    [TrazosIcons]::WriteIco($name, (Join-Path $dir "$name.ico"))
}
if ($Preview) {
    [TrazosIcons]::WritePreview((Join-Path $dir 'preview.png'))
}
Get-ChildItem $dir -Filter *.ico | Select-Object Name, Length
