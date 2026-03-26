import java.io.BufferedWriter;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;
import java.io.Writer;

public class SinGen {

	public static class PWLOut {
		Writer writer;
		double lastValue = 0;
		double lastTime = 0;
		boolean lastWritePostponed;
		public PWLOut(String fn) throws UnsupportedEncodingException, FileNotFoundException {
			writer = new BufferedWriter(new OutputStreamWriter(
		              new FileOutputStream(fn), "utf-8"));
		}
		public void write(double t, double v) throws IOException {
			if (t == 0 || lastValue != v) {
				if (lastWritePostponed) {
					writer.write( String.format("%.9f\t%.6f\n", lastTime, lastValue) );
				}
				writer.write( String.format("%.9f\t%.6f\n", t, v) );
				lastValue = v;
			} else {
				lastWritePostponed = true;
			}
			lastTime = t;
		}
		public void write(double t) throws IOException {
			write(t, lastValue);
		}
		public void close() throws IOException {
			if (writer != null) {
				writer.close();
			}
		}
	}
	
	public static class PWLBus {
		PWLOut[] bitOutP;
		PWLOut[] bitOutN;
		int bits;
		double value0 = 0.15;
		double value1 = 3.3 - 0.15;
		double slew = 0.000_000_003;
		public int getBitCount() {
			return bits;
		}
		public PWLBus(int bits, String fnPrefix, String fnSuffix) throws UnsupportedEncodingException, FileNotFoundException {
			this.bits = bits;
			bitOutP = new PWLOut[bits];
			bitOutN = new PWLOut[bits];
			for (int i = 0; i < bits; i++) {
				bitOutP[i] = new PWLOut(fnPrefix + i + "P" + fnSuffix);
				bitOutN[i] = new PWLOut(fnPrefix + i + "N" + fnSuffix);
			}
		}
		public void close() throws IOException {
			for (int i = 0; i < bits; i++) {
				bitOutP[i].close();
				bitOutN[i].close();
			}
			
		}
		public void write(double t, int v) throws IOException {
			for (int i = 0; i < bits; i++) {
				double vp = (v & (1 << i)) != 0 ? value1 : value0;
				double vn = (v & (1 << i)) != 0 ? value0 : value1;
				if (t > 0) {
					bitOutP[i].write(t - slew);
					bitOutN[i].write(t - slew);
				}
				bitOutP[i].write(t, vp);
				bitOutN[i].write(t, vn);
			}
		}
	}
	
	public static class DDSSine {
		double slew = 0.000_000_003;
		int sampleRate = 24_000_000;
		double sampleStep = 1.0 / sampleRate;
		double frequency = 1_000_000;
		long phase = 0;
		long phaseIncrement = 0;
		final int phaseBits = 32;
		final long phaseMod = 1L << phaseBits;
		final long phaseMask = phaseMod - 1;
		PWLBus out;
		PWLOut outp;
		PWLOut outn;
		double outMiddle = 4.5 / 2;
		double outAmp = 2;
		int bits;
		final int sinTableSizeBits = 10;
		final int sinTableSize = 1 << sinTableSizeBits;
		final int sinTableMask = sinTableSize - 1;
		final int phaseShift = phaseBits - sinTableSizeBits;
		int[] sinTable = new int[sinTableSize];
		double time = 0;
		final double timeIncrement = 1.0  / sampleRate;
		public DDSSine(int bits, String fnPrefix, String fnSuffix) throws UnsupportedEncodingException, FileNotFoundException {
			out = new PWLBus(bits, fnPrefix, fnSuffix);
			outp = new PWLOut(fnPrefix + "P" + fnSuffix);
			outn = new PWLOut(fnPrefix + "N" + fnSuffix);
			this.bits = bits;
			double middle = (1 << bits) / 2 - 0.5;
			double amp = (1 << bits) / 2 - 0.5;
			for (int i = 0; i < sinTableSize; i++) {
				double phaseRadians = (i + 0.5) * 2 * Math.PI / sinTableSize;
				double sinValue = Math.sin(phaseRadians);
				// 256/2 = 128
				int v = (int)Math.round( sinValue * amp + middle );
				sinTable[i] = v;
			}
			phase = 0;
			
		}
		public void setFrequency(double freq) {
			frequency = freq;
			phaseIncrement = Math.round(phaseMod * frequency / sampleRate);
		}
		public void tick() throws IOException {
			int tableIndex = ((int)(phase >> phaseShift)) & sinTableMask;
			int value = sinTable[tableIndex];
			int nvalue = value ^ ((1 << bits) - 1);
			double valueVoltageP = outMiddle + outAmp * (value - ((1 << bits)/2.0 - 0.5)) / ((1 << bits)/2.0);
			double valueVoltageN = outMiddle + outAmp * (nvalue - ((1 << bits)/2.0 - 0.5)) / ((1 << bits)/2.0);
			if (time > 0) {
				outp.write(time - slew);
				outn.write(time - slew);
			}
			outp.write(time, valueVoltageP);
			outn.write(time, valueVoltageN);
			out.write(time, value);
			time = time + timeIncrement;
			phase = (phase + phaseIncrement) & phaseMask;
		}
		public void sweepFrequency(double targetTime, double targetFrequency) throws IOException {
			double startTime = time;
			double endTime = targetTime;
			double startFreq = frequency;
			double endFreq = targetFrequency;
			while (time < targetTime) {
				tick();
				double f = startFreq + (time - startTime)/(endTime-startTime) * (endFreq - startFreq);
				setFrequency(f);
			}
		}
		public void close() throws IOException {
			out.close();
			outp.close();
			outn.close();
		}
	}
	
	public static void main(String[] args) {
		try {
			DDSSine dds = new DDSSine(8, "sin_sweep_dac_8bit_", ".pwl");
			dds.setFrequency(1_000_000.0);
			dds.sweepFrequency(0.001, 1_000_000.0);
			dds.sweepFrequency(0.002, 1_050_000.0);
			dds.sweepFrequency(0.003, 1_050_000.0);
			dds.sweepFrequency(0.004, 1_060_000.0);
			dds.sweepFrequency(0.005, 1_060_000.0);
			dds.sweepFrequency(0.006, 1_070_000.0);
			dds.sweepFrequency(0.007, 1_070_000.0);
			dds.sweepFrequency(0.008, 1_080_000.0);
			dds.sweepFrequency(0.009, 1_080_000.0);
			dds.sweepFrequency(0.010, 1_090_000.0);
			dds.sweepFrequency(0.011, 1_090_000.0);
			dds.sweepFrequency(0.012, 1_140_000.0);
			dds.sweepFrequency(0.013, 1_140_000.0);
			for (int i = 0; i < 100; i++) {
				dds.tick();
			}
			dds.close();
		} catch (UnsupportedEncodingException | FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
}
