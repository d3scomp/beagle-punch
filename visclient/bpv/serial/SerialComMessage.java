package bpv.serial;

import java.lang.*;

/*
Represents a message to be sent, or a received message through Serial port.
The message contained is always valid.
*/
public class SerialComMessage
{
	public static final int MAX_LENGTH = (1 << 14) - 1;
	private byte[] data = new byte[0];
	private int messageType;
	private int messageNumber;

	public void setType(int msgType)
	{
		this.messageType = msgType & 0x7f;
	}

	public int getType()
	{
		return this.messageType;
	}

	public void setNumber(int number)
	{
		this.messageNumber = number & 0x7f;
	}

	public int getNumber()
	{
		return this.messageNumber;
	}

	public void setData(byte[] data)
	{
		if (data.length > MAX_LENGTH)
		{
			throw new IllegalArgumentException("Too much data.");
		}

		this.data = data;
	}

	public byte[] getData()
	{
		return this.data;
	}

	public byte checksum()
	{
		byte cs = (byte)(this.data.length + (this.data.length >> 7) + this.messageType + this.messageNumber);
		for (byte x : this.data)
		{
			cs += 0xff & x;
		}

		return (byte)(cs & 0x7f);
	}
}

