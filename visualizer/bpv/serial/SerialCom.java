package bpv.serial;

import java.lang.*;
import java.io.*;
import java.util.*;
import jssc.*;

/*
	Message structure: MSG_START=0xa5,checksum=msg len + msg type + sum of data,data_len -> max 255B,msg type 
	max msg length = 1024
*/
public class SerialCom implements Closeable
{
	private static final byte MSG_START = (byte)0xa5;

	public static final int MT_ECHO = 1;
	public static final int MT_ECHO_RESPONSE = 2;
	public static final int MT_DATA = 3;
	public static final int MT_RESPONSE = 4;

	private HashSet<Integer> messageTypes = new HashSet<Integer>();
	private ArrayList<SerialComEventListener> listeners = new ArrayList<SerialComEventListener>();
	private SerialPort sp;
	private int msTimeout = 100; // timeout after which message receiver resets
	private MessageReader reader;

	public SerialCom(String portName) throws SerialPortException
	{
		this.addMessageType(MT_ECHO);
		this.addMessageType(MT_ECHO_RESPONSE);
		this.addMessageType(MT_DATA);
		this.addMessageType(MT_RESPONSE);

		this.sp = new SerialPort(portName);
		boolean ok = this.sp.openPort();
		ok = ok && this.sp.setParams(SerialPort.BAUDRATE_115200, SerialPort.DATABITS_8, SerialPort.STOPBITS_1, SerialPort.PARITY_NONE);
		ok = ok && this.sp.purgePort(SerialPort.PURGE_RXCLEAR | SerialPort.PURGE_TXCLEAR);
		if (!ok)
		{
			throw new SerialPortException(portName, "SerialCom constructor", "Unable to open or setup serial port.");
		}

		this.reader = new MessageReader();
		this.sp.addEventListener(this.reader, SerialPort.MASK_RXCHAR);
	}
	
	public void close()
	{
		try
		{
			this.sp.closePort();
		}
		catch (SerialPortException e)
		{
			System.err.println(e);
		}
	}

	public void addListener(SerialComEventListener listener)
	{
		this.listeners.add(listener);
	}

	public void addMessageType(int type)
	{
		this.messageTypes.add(type);
	}

	private boolean messageValidForTransmit(SerialComMessage m)
	{
		return this.messageTypes.contains(m.getType());
	}

	public boolean sendMessage(SerialComMessage m)
	{
		if (!this.messageValidForTransmit(m))
		{
			return false;
		}

		byte data[] = m.getData();
		byte header[] = new byte[6];
		header[0] = MSG_START;
		header[1] = m.checksum();
		header[2] = (byte)(data.length >> 7);
		header[3] = (byte)(data.length & 0x7f);
		header[4] = (byte)m.getType();
		header[5] = (byte)m.getNumber();

		try
		{
			this.sp.writeBytes(header);
			this.sp.writeBytes(data);
		}
		catch (SerialPortException e)
		{
			System.err.println(e);
			return false;
		}

		return true;
	}

	private boolean receivedMessageValid(SerialComMessage m, byte checksum)
	{
		return this.messageTypes.contains(m.getType()) && checksum == m.checksum();
	}

	private void messageReceived(SerialComMessage m, byte checksum)
	{
		if (this.receivedMessageValid(m, checksum))
		{
			for (SerialComEventListener l : this.listeners)
			{
				l.messageReceived(m);
			}
		}
	}

	private enum ReaderState
	{
		BETWEEN, HEADER, DATA
	}
	/* encapsulates a current state in reading of message */
	private class MessageReader implements SerialPortEventListener
	{
		private ReaderState state = ReaderState.BETWEEN;
		private int remaining;
		private ArrayList<Byte> header = new ArrayList<Byte>();
		private ArrayList<Byte> data = new ArrayList<Byte>();
		private	long lastEventTime = 0; 

		public void serialEvent(SerialPortEvent event)
		{
			assert(event.isRXCHAR());
			// event.isRXCHAR() // whether it is an rx event
			// event.getEventValue() // number of bytes in buffer for rx event
			byte[] buffer = null;
			try
			{
				buffer = SerialCom.this.sp.readBytes(); // read all received bytes
			}
			catch(SerialPortException e)
			{
				System.err.println(e);
				this.state = ReaderState.BETWEEN;
				return;
			}

			long currentTime = System.currentTimeMillis();
			if (currentTime - this.lastEventTime > SerialCom.this.msTimeout)
			{
				this.state = ReaderState.BETWEEN;
			}

			this.lastEventTime = currentTime;
			this.processBuffer(buffer);	
		}

		private void messageReceived()
		{
			SerialComMessage m = new SerialComMessage();
			m.setType(this.header.get(4));
			m.setNumber(this.header.get(5));
			byte[] data = new byte[this.data.size()];
			for (int i = 0; i < data.length; ++i)
			{
				data[i] = this.data.get(i);
			}

			m.setData(data);
			SerialCom.this.messageReceived(m, (byte)(this.header.get(1) & 0x7f));
		}

		private void processBuffer(byte[] buffer)
		{
			int i = 0;
			while (i < buffer.length)
			{
				int end;
				switch (this.state)
				{
					case BETWEEN:
						while (i < buffer.length && buffer[i] != MSG_START)
						{
							++i;
						}

						if (i < buffer.length && buffer[i] == MSG_START)
						{
							this.header.clear();
							this.header.add(MSG_START);
							this.data.clear();
							this.remaining = 5; // only 3, we've already read the first byte of the header
							this.state = ReaderState.HEADER;
							++i;
						}
					break;
					case HEADER:
						end = i + Math.min(this.remaining, buffer.length - i);
						while (i < end)
						{
							this.header.add(buffer[i]);
							++i;
							--this.remaining;
						}

						if (this.remaining == 0)
						{
							this.remaining = ((this.header.get(2) & 0x7f) << 7) | (this.header.get(3) & 0x7f);
							this.state = ReaderState.DATA;
						}
					break;
					case DATA:
						end = i + Math.min(this.remaining, buffer.length - i);
						while (i < end)
						{
							this.data.add(buffer[i]);
							++i;
							--this.remaining;
						}

						if (this.remaining == 0)
						{
							this.state = ReaderState.BETWEEN;
							this.messageReceived();
						}
					break;
				}
			}
		}
	}
}

