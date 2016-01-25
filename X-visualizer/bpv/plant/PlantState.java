package bpv.plant;

import java.lang.*;
import java.util.*;

import java.nio.charset.*;
import java.nio.file.*;
import java.io.*;

import javax.json.*;
import javax.json.stream.*;

import bpv.serial.*;

public class PlantState implements Closeable, SerialComEventListener
{
	private static final double POSITION_DIVIDER = 1000000;
	private static final double VELOCITY_DIVIDER = 1000000;
	private static final SessionId INVALID_ID = new SessionId("SURELY_INVALID_ID");
	private static final int POSITION_LOG_PERIOD = 40;

	private SerialCom sc;
	private BufferedWriter logWriter;

	/* state of the plant updated by the status command */

	private List<Punch> planned = new ArrayList<Punch>();
	private List<Punch> punched = new ArrayList<Punch>();
	private int prevPunchCount = 0;
	private boolean fail;
	private double xpos;
	private double ypos;
	private double xvelocity;
	private double yvelocity;

	/* state of the plant updated by the session command */

	private SessionId sessionId = INVALID_ID;
	private boolean initPosRandom = true;
	private double xInitPos;
	private double yInitPos;

	private Object updateMutex = new Object();
	private int updateNumber = 0;
	private boolean updateCompleted = false;
	private int sessionUpdateNumber = 0;

	private int positionLogCounter = 0;

	private Object msgSendMutex = new Object();
	
	private void send(SerialComMessage m)
	{
		synchronized(this.msgSendMutex)
		{
			this.sc.sendMessage(m);
		}
	}

	public PlantState(SerialCom sc)
	{
		this.sc = sc;
		this.sc.addListener(this);
	}

	public List<Punch> getPlannedPunches()
	{
		return Collections.unmodifiableList(this.planned);
	}

	public List<Punch> getPunchedPunches()
	{
		return Collections.unmodifiableList(this.punched);
	}

	public boolean isOK()
	{
		return !this.fail;
	}

	public boolean previousUpdateCompleted()
	{
		return this.updateCompleted;
	}

	public double getXPos()
	{
		return this.xpos;
	}

	public double getYPos()
	{
		return this.ypos;
	}

	public double getXVelocity()
	{
		return this.xvelocity;
	}

	public double getYVelocity()
	{
		return this.yvelocity;
	}

	public SessionId getSessionId()
	{
		return this.sessionId;
	}

	public boolean isInitPosRandom()
	{
		return this.initPosRandom;
	}

	public double getXInitPos()
	{
		return this.xInitPos;
	}

	public double getYInitPos()
	{
		return this.yInitPos;
	}
	
	public void loadPlannedPunches(String fileName) throws IOException
	{
		Path p = Paths.get(fileName);
		Charset charset = Charset.forName("UTF-8");
		try (BufferedReader r = Files.newBufferedReader(p, charset))
		{
			String line = null;
			while ((line = r.readLine()) != null)
			{
				//System.out.println(line);
				String[] coordinates = line.split(",", 2);
				if (coordinates.length != 2)
				{
					continue;
				}

				try
				{
					double x = Double.parseDouble(coordinates[0]);
					double y = Double.parseDouble(coordinates[1]);
					this.planned.add(new Punch(x, y));
				}
				catch (NumberFormatException e)
				{
				}
			}
		}
		catch (IOException e)
		{
		}
	}

	private void log(String line)
	{
		System.out.println(line);
		if (this.logWriter != null)
		{
			try
			{
				line = line + "\n";
				this.logWriter.write(line, 0, line.length());
			}
			catch (IOException e)
			{
			}
		}
	}

	public void setLogFile(String fileName) throws IOException
	{
		if (this.logWriter != null)
		{
			this.logWriter.close();
		}

		Charset charset = Charset.forName("UTF-8");
		this.logWriter = Files.newBufferedWriter(Paths.get(fileName), charset);
	}

	public void close() throws IOException
	{
		if (this.logWriter != null)
		{
			this.logWriter.close();
		}
	}

	public void setInitPosRandom()
	{
		final String command = "{\"command\":\"random_initpos\"}";
		SerialComMessage m = new SerialComMessage();
		m.setType(SerialCom.MT_DATA);
		m.setNumber(0);
		m.setData(command.getBytes(StandardCharsets.US_ASCII));
		this.send(m);
	}

	private int doublePosition2Int(double pos)
	{
		return (int)(pos * 4) * (int)(POSITION_DIVIDER / 4);
	}

	public void setInitPos(double x, double y)
	{
		final String command = String.format("{\"command\":\"initpos\",\"x\":%d,\"y\":%d}", this.doublePosition2Int(x), this.doublePosition2Int(y));
		SerialComMessage m = new SerialComMessage();
		m.setType(SerialCom.MT_DATA);
		m.setNumber(0);
		m.setData(command.getBytes(StandardCharsets.US_ASCII));
		this.send(m);
	}

	public void update()
	{
		int msgNumber;
		synchronized(this.updateMutex)
		{
			this.updateCompleted = false;
			msgNumber = (this.updateNumber = (this.updateNumber + 1) & 0x3f);
		}

		final String command = "{\"command\":\"status\"}";
		SerialComMessage m = new SerialComMessage();
		m.setType(SerialCom.MT_DATA);
		m.setNumber(msgNumber);
		m.setData(command.getBytes(StandardCharsets.US_ASCII));

		this.send(m);

		if (msgNumber % 7 == 0)
		{
			msgNumber |= 0x40;
			this.sessionUpdateNumber = msgNumber;

			final String sessionCommand = "{\"command\":\"session\"}";
			m = new SerialComMessage();
			m.setType(SerialCom.MT_DATA);
			m.setNumber(msgNumber);
			m.setData(sessionCommand.getBytes(StandardCharsets.US_ASCII));
			this.send(m);
		}
	}

	private Position readPosition(JsonObject root, String propName)
	{
		JsonObject pos = root.getJsonObject(propName);
		if (pos != null)
		{
			Position p = new Position();
			p.X = pos.getInt("x");
			p.Y = pos.getInt("y");
			return p;
		}

		return null;
	}

	private double jsonPosition2Double(int pos)
	{
		return (double)pos / POSITION_DIVIDER;
	}

	private double jsonVelocity2Double(int vel)
	{
		return (double)vel / VELOCITY_DIVIDER;
	}

	private void processStatusMessage(String status)
	{
		try
		{
			JsonReader reader = Json.createReader(new StringReader(status));
			JsonObject root = reader.readObject();

			Position pos = this.readPosition(root, "pos");
			this.xpos = this.jsonPosition2Double(pos.X);
			this.ypos = this.jsonPosition2Double(pos.Y);
			//System.out.println(String.format("%f %f", this.xpos, this.ypos));
	
			pos = this.readPosition(root, "vel");
			this.xvelocity = this.jsonPosition2Double(pos.X);
			this.yvelocity = this.jsonPosition2Double(pos.Y);		

			int punchCount = root.getInt("num");
			if (punchCount > this.prevPunchCount)
			{
				this.prevPunchCount = punchCount;
				pos = this.readPosition(root, "punch");
				Punch p = new Punch(this.jsonPosition2Double(pos.X), this.jsonPosition2Double(pos.Y));
				this.punched.add(p);
				this.log(String.format("PUNCH: x: %.2f y: %.2f", p.getX(), p.getY()));
			}
				
			boolean prevFail = this.fail;
			this.fail = root.getBoolean("fail");
			if (!prevFail && this.fail)
			{
				this.log("FAIL!");
			}

			reader.close();

			++this.positionLogCounter;
			if (this.positionLogCounter > POSITION_LOG_PERIOD)
			{
				this.positionLogCounter = 0;
				this.log(String.format("position: x: %.2f y: %.2f", this.xpos, this.ypos));
			}
		}
		catch (JsonException e) // readObject error
		{
			System.err.println(e);
		}
		catch (NullPointerException e) // property read error
		{
			System.err.println(e);
		}
		catch (ClassCastException e) // property read error
		{
			System.err.println(e);
		}
	}

	private void processSessionMessage(String session)
	{
		//System.out.println(status);
		try
		{
			JsonReader reader = Json.createReader(new StringReader(session));
			JsonObject root = reader.readObject();

			SessionId newId = new SessionId(root.getString("id"));
			if (!this.sessionId.equals(newId) && !this.sessionId.equals(INVALID_ID))
			{
				this.punched.clear();
				this.prevPunchCount = 0;
				this.log("RESET");
			}

			this.sessionId = newId;

			this.initPosRandom = root.getBoolean("rnd");

			Position pos = this.readPosition(root, "ip");
			this.xInitPos = this.jsonPosition2Double(pos.X);
			this.yInitPos = this.jsonPosition2Double(pos.Y);

			reader.close();

		}
		catch (JsonException e) // readObject error
		{
			System.err.println(e);
		}
		catch (NullPointerException e) // property read error
		{
			System.err.println(e);
		}
		catch (ClassCastException e) // property read error
		{
			System.err.println(e);
		}
	}

	public void messageReceived(SerialComMessage m)
	{
		if (m.getType() != SerialCom.MT_RESPONSE)
		{
			return;
		}

		boolean statusMessage = false;
		synchronized(this.updateMutex)
		{
			if (m.getNumber() == this.updateNumber)
			{
				this.updateCompleted = true;
				statusMessage = true;
			}
		}

		String json = new String(m.getData(), StandardCharsets.US_ASCII);
		if (statusMessage)
		{
			this.processStatusMessage(json);
		}
		else if (m.getNumber() == this.sessionUpdateNumber)
		{
			this.processSessionMessage(json);
		}

	}

	private static class Position
	{
		public int X;
		public int Y;
	}
}

