package bpv.view;

import javafx.application.*;
import javafx.geometry.*;
import javafx.event.*;
import javafx.scene.*;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.*;
import javafx.scene.shape.*;
import javafx.scene.paint.*;
import javafx.scene.canvas.*;
import javafx.scene.text.*;
import javafx.animation.*;
import javafx.util.*;
import javafx.beans.value.*;

import java.util.*;
import java.io.*;
import java.lang.*;

import bpv.plant.*;

public class PunchView extends ScrollPane
{
	// right panel with punches
	private List<PlannedPunch> planned = new ArrayList<PlannedPunch>();
	private VBox punchedBox;
	private int punchCount = 0;
	private SessionId sessionId = new SessionId("certainlyNotAValidSessionId");

	public PunchView(Iterable<Punch> plannedPunches)
	{
		VBox punchBox = new VBox(5);
		this.setContent(punchBox);

		VBox plannedBox = new VBox();
		for (Punch p : plannedPunches)
		{
			PlannedPunch pp = new PlannedPunch(p);
			plannedBox.getChildren().add(pp.label);
			this.planned.add(pp);
		}

		this.punchedBox = new VBox();
		
		Separator s = new Separator();
		s.setOrientation(Orientation.HORIZONTAL);
		Label l1 = new Label("Planned punches:");
		l1.setFont(Font.font(null, FontWeight.BOLD, 14));
		Label l2 = new Label("Punched punches:");
		l2.setFont(Font.font(null, FontWeight.BOLD, 14));

		punchBox.getChildren().addAll(l1, plannedBox, s, l2, this.punchedBox);
	}

	public void update(PlantState plant)
	{
		SessionId newSessionId = plant.getSessionId();
		if (!newSessionId.equals(this.sessionId))
		{
			this.sessionId = newSessionId;
			this.punchCount = 0;
			this.punchedBox.getChildren().clear();
			for (PlannedPunch pp : this.planned)
			{
				pp.reset();
			}
		}

		List<Punch> punches = plant.getPunchedPunches();
		for (int i = this.punchCount; i < punches.size(); ++i)
		{
			Punch p = punches.get(i);
			Label l = new Label(String.format("x: %.2f, y: %.2f", p.getX(), p.getY()));
			this.punchedBox.getChildren().add(l);

			for (PlannedPunch pp : this.planned)
			{
				pp.check(p.getX(), p.getY());
			}
		}

		this.punchCount = punches.size();
	}

	private static class PlannedPunch
	{
		private static final Paint missCol = Color.RED;
		private static final Paint hitCol = Color.GREEN;

		public int x, y;
		public Label label;

		public PlannedPunch(Punch p)
		{
			double x = p.getX();
			double y = p.getY();
			this.x = toInt(x);
			this.y = toInt(y);
			this.label = new Label(String.format("x: %.2f, y: %.2f", x, y));
			this.reset();
		}

		public void reset()
		{
			this.label.setTextFill(missCol);
		}

		public void check(double x, double y)
		{
			if (this.x == toInt(x) && this.y == toInt(y))
			{
				this.label.setTextFill(hitCol);
			}
		}

		private static int toInt(double pos)
		{
			return (int)(pos * 4);
		}
	}
}
