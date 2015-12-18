package bpv.plant;

import java.lang.*;

public class SessionId
{
	private String id = null;

	public SessionId(String id)
	{
		this.id = id;
	}

	public boolean equals(SessionId other)
	{
		if (this.id == null || other.id == null)
		{
			return this.id == other.id;
		}

		return this.id.equals(other.id);
	}
}
