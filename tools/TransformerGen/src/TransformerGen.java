import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;
import java.util.UUID;

/**
 * KiCAD Footprint generator for round planar transformer.
 * To be used in Theremin Sine Wave Analog PLL Current Sensing Oscillator project. 
 */
public class TransformerGen {

	// parameters of planar PCB transformer footprint to generate
	// TBD: place to .ini file, commandline parameters, or GUI
	public static class Options {
		//
		String footprintName = "CurrSensTrans1";
		int traceWidth1 = 160_000;
		int traceWidth2 = 80_000;
		int step = 400_000;
		int innerRadius = 3_000_000;
		// number of turns for single spiral coil, in half-turns
		int halfTurnsCount = 4 * 2 + 1;
		// pad parameters
		int padSize = 1_000_000; //1_600_000;
		int padDrillSize = 600_000; //800_000;
		// coil end rounding radius
		int endRadius = 800_000;
		String coil1PinName1 = "2";
		String coil1PinName2 = "1";
		String coil2PinName1 = "3";
		String coil2PinName2 = "4";
		
		// set to 0 if you don't need internal hole
		int internalCutHoleRadius = 1_500_000;
		int cutLineWidth = 50_000;
		String cutLineType = "default";
		int cornerCutHoleSize = 5_750_000;
		int cornerCutHoleWidth = 4_000_000;
		int cornerCutHoleRadius = 5_400_000;
		int cornerCutHoleRounding = 500_000;
	}
	
	String layer = "F.Cu";
	
	public void setLayer(String layer) {
		this.layer = layer;
	}

	public static String genUUID() {
		UUID uuid = UUID. randomUUID();
		return quote(uuid.toString());
	}
	
	public void putUUID() {
		putProp(2, "uuid", genUUID());
	}
	
	public static String quote(String s) {
		return "\"" + s + "\"";
	}
	
	/// format fixed point *1000000 number
	public static String fmt(int v) {
		StringBuilder buf = new StringBuilder();
		//buf.append((double)v / 1_000_000);
		int a = v / 1_000_000;
		int b = v < 0 ? -v : v;
		b = b % 1_000_000;
		if (a == 0 && v < 0) {
			buf.append("-0");
		} else {
			buf.append(a);
		}
		if (b != 0) {
			buf.append('.');
			String digits = String.valueOf(b);
			for (int i = digits.length(); i < 6; i++) {
				buf.append('0');
			}
			if (b % 100_000 == 0) {
				buf.append(b / 100_000);
			} else if (b % 10_000 == 0) {
				buf.append(b / 10_000);
			} else if (b % 1_000 == 0) {
				buf.append(b / 1_000);
			} else if (b % 100 == 0) {
				buf.append(b / 100);
			} else if (b % 10 == 0) {
				buf.append(b / 10);
			} else {
				buf.append(b);
			}
		}
		return buf.toString();
	}
	
	public void putProp(int ident, String name, Object ... values) {
		StringBuilder buf = new StringBuilder();
		buf.append('(');
		buf.append(name);
		for (int i = 0; i < values.length; i++) {
			buf.append(' ');
			Object v = values[i];
			if (v instanceof Integer) {
				buf.append(fmt((Integer)v));
			} else {
				buf.append(v.toString());
			}
		}
		buf.append(')');
		puts(ident, buf.toString() );
	}

	public void putFont(int size, int thickness) {
		puts(2, "(effects");
		puts(3, "(font");
		putProp(4, "size", size, size);
		putProp(4, "thickness", thickness);
		puts(3, ")");
		puts(2, ")");
	}
	
	public void putStroke(int width, String type) {
		puts(2, "(stroke");
		putProp(3, "width", width);
		putProp(3, "type", type);
		puts(2, ")");
	}
	
	public void putLayer(String layer) {
		putProp(2, "layer", quote(layer));
	}
	
	public void putAt(int a, int b, int c) {
		putProp(2, "at", a, b, c);
	}
	
	public void putPropQ(String name, String value) {
		puts(2, "(" + name + " \"" + value + "\")" );
	}

	public void startProperty(String name, String value) {
        puts(1, "(property " + quote(name) + " " + quote(value));
	}
	
	public void endProperty() {
        puts(1, ")");
	}
	
	public void putReference(String value) {
        startProperty("Reference", value);
        putProp(2, "at", 0, 500_000 - options.innerRadius, 0);
        putProp(2, "unlocked", "yes");
        putLayer("F.SilkS");
        putUUID();
        putFont(1000_000, 100_000);
        endProperty();
	}

	public void putValue(String value) {
        startProperty("Value", value);
        putProp(2, "at", 0, 1000_000, 0);
        putProp(2, "unlocked", "yes");
        putLayer("F.Fab");
        putUUID();
        putFont(1000_000, 150_000);
        endProperty();
	}

	public void putDatasheet(String value) {
        startProperty("Datasheet", value);
        putProp(2, "at", 0, 0, 0);
        putProp(2, "unlocked", "yes");
        putLayer("F.Fab");
        putProp(2, "hide", "yes");
        putUUID();
        putFont(1000_000, 150_000);
        endProperty();
	}

	public void putDescription(String value) {
        startProperty("Description", value);
        putProp(2, "at", 0, 0, 0);
        putProp(2, "unlocked", "yes");
        putLayer("F.Fab");
        putProp(2, "hide", "yes");
        putUUID();
        putFont(1000_000, 150_000);
        endProperty();
	}

	public void putText(String value, int a, int b, int c) {
        puts(1, "(fp_text user " + quote(value));
        putProp(2, "at", a, b, c);
        putProp(2, "unlocked", "yes");
        putLayer(layer);
        //putProp(2, "hide", "yes");
        putUUID();
        putFont(1000_000, 150_000);
        endProperty();
	}

	public void genFooter() {
		setLayer("F.Fab");
		putText("${REFERENCE}", 0, 2500_000, 0);
		puts(1, "(embedded_fonts no)");
		puts(0, ")");
	}
	
	public void genHeader() {
        puts(1, "(version 20241229)");
        puts(1, "(generator \"pcbnew\")");
        puts(1, "(generator_version \"9.0\")");
        puts(1, "(layer \"F.Cu\")");
        putReference("Ref**");
        putValue(options.footprintName);
        putDatasheet("");
        putDescription("");
        puts(1, "(attr through_hole)");
		
	}
	
	public void putArc(int thickness, int x0, int y0, int x1, int y1, int x2, int y2) {
        puts(1, "(fp_arc");
        putProp(2, "start", x0, y0);
        putProp(2, "mid", x1, y1);
        putProp(2, "end", x2, y2);
        
        putStroke(thickness, "solid");
        putLayer(layer);
        putUUID();
        puts(1, ")");
	}

	public void genCoils() {
		setLayer("F.Cu");
		
	}
	
	public void putPad(int x, int y, String name) {
        puts(1, "(pad " + quote(name) + " thru_hole circle");
        putProp(2, "at", x, y);
        putProp(2, "size", options.padSize, options.padSize);
        putProp(2, "drill", options.padDrillSize);
        putProp(2, "layers", quote("*.Cu"), quote("*.Mask"));
        putProp(2, "remove_unused_layers", "no");
        putUUID();
        puts(1, ")");
/*
        (pad "2" thru_hole circle
                (at 7.5 5.5)
                (size 1.6 1.6)
                (drill 0.8)
                (layers "*.Cu" "*.Mask")
                (remove_unused_layers no)
                (uuid "cda7c348-5268-42db-b523-e55e516b4410")
        )
		
 */
	}
	
	public int mulSin45(int rr) {
		return (int)(rr * 707_107L / 1_000_000);
	}
	public void putSpiral(int x, int y, int width, int innerRadius, int step, int halfTurnCount) {
		int radius = innerRadius + step/2;
		int rr = options.endRadius;
		int rr45 = mulSin45(rr);
		
		putArc(width, 
				x+radius - rr, y - rr,
				x+radius - rr + rr45, y-rr45,
				x+radius, y
				);
		putPad(x+radius - rr, y - rr, options.coil1PinName2);
		
		for (int i = 0; i < halfTurnCount; i++) {
			if ((i % 2) == 0) {
				putArc(width, x + radius, y, 
						x, y+radius, 
						x - radius, y);
			} else {
				putArc(width, x - radius, y, 
						x + step/2, y-radius-step/2, 
						x + radius + step, y);
				radius += step;
			}
		}

		putArc(width, 
				x-radius - rr, y - rr,
				x-radius - rr + rr45, y-rr45,
				x-radius, y
				);
		putPad(x-radius - rr, y - rr, options.coil1PinName1);
	}
	
	public void putInnerSpiral(int x, int y, int width, int innerRadius, int step, int halfTurnCount) {
		int radius = innerRadius;
		int rr = options.endRadius;
		
		// sin(45)=0.707107
		int r45 = mulSin45(radius);
		
		// connector
		int rr45 = mulSin45(rr);
		
		putArc(width, 
				x, y+radius,
				x+rr45, y+radius-rr+rr45, 
				x+rr, y+radius-rr
				);
		putPad(x+rr, y+radius-rr, options.coil2PinName2);

		
		putArc(width, 
				x, y+radius,
				x-r45, y+r45, 
				x-radius, y
				);
		int lastx = 0; 
		int lasty = 0;
		for (int i = 1; i < halfTurnCount; i++) {
			if ((i % 2) == 0) {
				putArc(width, 
						x + radius, y, 
						x, y+radius, 
						x - radius, y);
				lastx = x - radius;
				lasty = y;
			} else {
				putArc(width,
						x - radius, y, 
						x + step/2, y-radius-step/2, 
						x + radius + step, y);
				lastx = x + radius + step;
				lasty = y;
				radius += step;
			}
		}
		if (halfTurnCount % 2 == 0) {
			r45 = (int)(radius * 707_107L / 1_000_000);
			putArc(width, 
					lastx, lasty,
					x + r45, y + r45,
					x, y + radius
					);
			lastx = x;
			lasty = y + radius;

			r45 = (int)(rr * 707_107L / 1_000_000);
			putArc(width, 
					lastx - rr, lasty + rr,
					lastx - r45, lasty + rr - r45,
					lastx, lasty
					);
			putPad(lastx - rr, lasty + rr, options.coil2PinName1);
		
		
		} else {
			//radius += step; + step/2
			r45 = (int)((radius + step/2) * 707_107L / 1_000_000);
			putArc(width, 
					lastx, lasty,
					x + step/2 - r45, y - r45,
					x + step/2, y - radius - step/2
					);
			lastx = x + step/2;
			lasty = y - radius - step/2;

			r45 = (int)(rr * 707_107L / 1_000_000);
			putArc(width, 
					lastx - rr,  lasty + rr,
					lastx - r45, lasty + rr - r45,
					lastx,       lasty
					);
			
		}
		
	}
	
	public void putLine(int x0, int y0, int x1, int y1, int width, String type) {
        puts(1, "(fp_line");
        putProp(2, "start", x0, y0);
        putProp(2, "end", x1, y1);
        putStroke(width, type);
        putProp(2, "layer", layer);
        putUUID();
        puts(1, ")");
	}
	
	public static int sqrt(long v) {
		double d = v / 1000000.0;
		return (int)(Math.sqrt(d) * 1000_000);
	}
	
	/**
	 * Quadrants:
	 * 
	 *    2 | 3
	 *    --+--
	 *    1 | 0
	 * 
	 * 
	 * @param width
	 * @param centerx
	 * @param centery
	 * @param rr
	 * @param quadrant
	 */
	public void putQArc(int width, int centerx, int centery, int rr, int quadrant) {
		quadrant = quadrant & 3;
		int rr45 = mulSin45(rr);
		if (quadrant == 0) {
			putArc(options.cutLineWidth, 
					centerx + rr, centery, 
					centerx + rr45, centery + rr45, 
					centerx, centery + rr);
		} else if (quadrant == 1) {
			putArc(options.cutLineWidth, 
					centerx, centery + rr, 
					centerx - rr45, centery + rr45, 
					centerx - rr, centery);
		} else if (quadrant == 2) {
			putArc(options.cutLineWidth,
					centerx - rr, centery,
					centerx - rr45, centery - rr45,
					centerx, centery - rr);
		} else if (quadrant == 3) {
			putArc(options.cutLineWidth, 
					centerx, centery - rr, 
					centerx + rr45, centery - rr45, 
					centerx + rr, centery);
		}
	}

	public void putCornerHole(int quadrant) {
		quadrant = quadrant & 3;
		int sx = 1;
		int sy = 1;
		if (quadrant == 0) {
			sx = 1; 
			sy = 1; 
		} else if (quadrant == 1) {
			sx = -1; 
			sy = 1; 
		} else if (quadrant == 2) {
			sx = -1; 
			sy = -1; 
		} else if (quadrant == 3) {
			sx = 1; 
			sy = -1; 
		}
		
		int sz = options.cornerCutHoleSize;
		int cornerx = sz * sx;
		int cornery = sz * sy;
		int rr = options.cornerCutHoleRounding;
		int rr45 = mulSin45(rr);
		int w = options.cornerCutHoleWidth;
		int r = options.cornerCutHoleRadius;
		int r45 = mulSin45(r);
		// corner
		putQArc(options.cutLineWidth, 
				cornerx - sx*rr, cornery - sy * rr, // center x,y 
				rr, quadrant);

		// top
		putLine(cornerx - sx * rr, cornery, cornerx - sx * (w - rr), cornery, options.cutLineWidth, "default");

		int flip = (quadrant & 1) << 1;
		putQArc(options.cutLineWidth, 
				cornerx - sx*(w - rr), cornery - sy*rr, // center x,y 
				rr, (quadrant + 1) ^ flip);
		
		// left
		putLine(cornerx, cornery - sy*rr, cornerx, cornery -sy * (w - rr), options.cutLineWidth, "default");

		putQArc(options.cutLineWidth, 
				cornerx - sx*rr, cornery - sy*(w - rr), // center x,y 
				rr, (quadrant - 1) ^ flip);

		// x^2 + y^2 == r^2
		// x == -sz + w
		// y == sqrt(r^2 - x^2)
		// y == sqrt(r^2 - (-sz + w)^2)
		int px = -sz + w;
		int dy = (int)Math.sqrt((double)r * r - (double)px*px);
		
		putLine(cornerx - sx * w, cornery - sy * rr,  cornerx - sx * w, sy * dy, options.cutLineWidth, "default");

		putLine(cornerx - sx * rr, cornery - sy * w,  
				sx * dy, cornery - sy * w, options.cutLineWidth, "default");

		
		int ax1 = sx * dy;
		int ay1 = cornery - sy * w;
		int ax2 = cornerx - sx * w;
		int ay2 = sy * dy;
		
		// center
		putArc(options.cutLineWidth, 
				ax1, ay1, 
				sx * r45, sy * r45,
				ax2, ay2);
		
		
		
	}
	
	public void putHoles() {
		setLayer("Edge.Cuts");
		if (options.internalCutHoleRadius > 0) {
			putCircle(0, 0, options.internalCutHoleRadius, options.cutLineWidth, options.cutLineType);
		}
		if (options.cornerCutHoleSize > 0) {
			putCornerHole(2);
			putCornerHole(0);
			putCornerHole(3);
			putCornerHole(1);
		}
	}
	
	public void putCircle(int x, int y, int radius, int width, String type) {
        puts(1, "(fp_circle");
        putProp(2, "center", x, y);
        putProp(2, "end", x + radius, y);
        putStroke(width, type);
        putProp(2, "fill", "no");
        putProp(2, "layer", layer);
        putUUID();
        puts(1, ")");
	}
	
	public void generateFile(String filename) throws IOException {
		openFile(filename);
		
		puts(0, "(footprint \"" + options.footprintName + "\"");
		genHeader();
		
		//putArc(120_000, 5_000, 0, 0, 5_000, -5_000, 0);
		putSpiral(0, 0,
				options.traceWidth1, 
				options.innerRadius, 
				400_000, options.halfTurnsCount);
		putInnerSpiral(0, 0,
				options.traceWidth2,
				options.innerRadius, 
				400_000, options.halfTurnsCount + 1);
		
		putHoles();
		
		genFooter();
		
		
		closeFile();
	}
	
	Options options = new Options();
	BufferedWriter writer;
	
	public void openFile(String filename)	 throws FileNotFoundException, UnsupportedEncodingException {
		FileOutputStream outstream = new FileOutputStream(new File(filename));
		writer = new BufferedWriter(new OutputStreamWriter(outstream, "utf-8"));
	}
	
	public void closeFile() throws IOException {
		writer.close();
	}
	
	public void puts(int ident, String line) {
		StringBuilder buf = new StringBuilder();
		for (int i = 0; i < ident; i++) {
			buf.append('\t');
		}
		buf.append(line);
		buf.append('\n');
		try {
			writer.write(buf.toString());
		} catch (IOException e) {
			throw new RuntimeException(e);
		}	
	}
	
	public static void main(String[] args) {
		TransformerGen gen = new TransformerGen();
		String filename = gen.options.footprintName + ".kicad_mod";
		System.out.println("Generating file " + filename);
		try {
			gen.generateFile(filename);
		} catch (IOException e) {
			e.printStackTrace();
		}
		System.out.println("Done");
	}

}
