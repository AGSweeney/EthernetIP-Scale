using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

class Program
{
    static void Main(string[] args)
    {
        if (args.Length != 2)
        {
            Console.WriteLine("Usage: ConvertToIco <pngPath> <icoPath>");
            return;
        }

        string pngPath = args[0];
        string icoPath = args[1];

        using (var png = new Bitmap(pngPath))
        {
            using (var ms = new MemoryStream())
            {
                png.Save(ms, ImageFormat.Png);
                ms.Position = 0;

                using (var fs = new FileStream(icoPath, FileMode.Create))
                using (var bw = new BinaryWriter(fs))
                {
                    bw.Write((short)0);
                    bw.Write((short)1);
                    bw.Write((short)1);
                    bw.Write((byte)(png.Width == 256 ? 0 : png.Width));
                    bw.Write((byte)(png.Height == 256 ? 0 : png.Height));
                    bw.Write((byte)0);
                    bw.Write((byte)0);
                    bw.Write((short)1);
                    bw.Write((short)32);
                    bw.Write((int)(png.Width * png.Height * 4 + 40));
                    bw.Write((int)22);
                    ms.Position = 0;
                    ms.CopyTo(fs);
                }
            }
        }

        Console.WriteLine($"Converted {pngPath} to {icoPath}");
    }
}

