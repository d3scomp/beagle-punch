package bpv.view;

import javafx.application.Application;
import javafx.geometry.*;
import javafx.event.*;
import javafx.scene.*;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.*;
import javafx.scene.shape.*;
import javafx.scene.paint.*;
import javafx.scene.canvas.*;
import javafx.animation.*;
import javafx.util.*;

import java.util.*;
import java.io.*;
import java.lang.*;

import jssc.*;

import bpv.plant.*;

public class PlantView extends ScrollPane
{
	public static final int BASE_WIDTH = 1500; // in mm
	public static final int BASE_HEIGHT = 1000; // in mm
	public static final int SAFE_ZONE_WIDTH = 20;
	public static final int PUNCH_SIZE = 10;

	private Canvas workArea;

	private List<HeadPart> headParts;

	private double zoom;
	private double punchSize;
	private double safeZoneWidth;
	private int punchCount = 0;
	private SessionId sessionId = new SessionId("certainlyNotAValidSessionId");

	public PlantView()
	{
		Pane g = new Pane();

		this.workArea = new Canvas(BASE_WIDTH + 2 * SAFE_ZONE_WIDTH, BASE_HEIGHT + 2 * SAFE_ZONE_WIDTH);
		g.getChildren().add(this.workArea);

		this.setContent(g);

		HeadPart part1 = new HeadPart(30, 4, 3, Color.TRANSPARENT); // size, strokewidth, round
		HeadPart part2 = new HeadPart(10, 1, 0, Color.GREEN);
		g.getChildren().add(part1.getNode());
		g.getChildren().add(part2.getNode());
		this.headParts = new ArrayList<HeadPart>();
		this.headParts.add(part1);
		this.headParts.add(part2);

		this.setZoom(1);
	}

	private void updateHead(PlantState plant)
	{
		double x = plant.getXPos();
		double y = plant.getYPos();
		for (HeadPart part : this.headParts)
		{
			part.updatePosition(x, y, false);
		}
	}

	private void drawSafeZones(GraphicsContext gc)
	{
		double width = 2 * this.safeZoneWidth + this.zoom * BASE_WIDTH;
		double height = 2 * this.safeZoneWidth + this.zoom * BASE_HEIGHT;
		gc.setFill(Color.RED);
		gc.fillRect(0, 0, width, this.safeZoneWidth);
		gc.fillRect(0, 0, this.safeZoneWidth, height);
		gc.fillRect(0, height - this.safeZoneWidth, width, this.safeZoneWidth);
		gc.fillRect(width - this.safeZoneWidth, 0, this.safeZoneWidth, height);
	}

	private void drawPunch(GraphicsContext gc, Punch p, Paint color)
	{
		gc.setFill(color);
		double x = p.getX();
		double y = p.getY();
		double halfSize = this.punchSize / 2;
		gc.fillRect(this.safeZoneWidth + x * this.zoom - halfSize, this.safeZoneWidth + y * this.zoom - halfSize, this.punchSize, this.punchSize);
	}

	private void redraw(PlantState plant, boolean all)
	{
		GraphicsContext gc = this.workArea.getGraphicsContext2D();
		if (all)
		{
			gc.setFill(Color.WHITE);
			gc.fillRect(0, 0, this.workArea.getWidth(), this.workArea.getHeight());
			this.drawSafeZones(gc);

			for (Punch p : plant.getPlannedPunches())
			{
				this.drawPunch(gc, p, Color.BROWN);
			}

			this.punchCount = 0;
		}

		List<Punch> punches = plant.getPunchedPunches();
		for (int i = this.punchCount; i < punches.size(); ++i)
		{
			this.drawPunch(gc, punches.get(i), Color.GREEN);
		}

		this.punchCount = punches.size();

		this.updateHead(plant);
	}

	public void redraw(PlantState plant)
	{
		this.redraw(plant, true);
	}

	public void update(PlantState plant)
	{
		SessionId newSessionId = plant.getSessionId();
		boolean all = !newSessionId.equals(this.sessionId);
		this.sessionId = newSessionId;
		this.redraw(plant, all);
	}

	public void setZoom(double zoom)
	{
		this.zoom = zoom;
		this.punchSize = PUNCH_SIZE * zoom;
		this.safeZoneWidth = SAFE_ZONE_WIDTH * zoom;
		this.workArea.setWidth(2 * this.safeZoneWidth + zoom * BASE_WIDTH);
		this.workArea.setHeight(2 * this.safeZoneWidth + zoom * BASE_HEIGHT);
		for (HeadPart part : this.headParts)
		{
			part.setZoom(zoom);
		}
	}

	private class HeadPart
	{
		private Rectangle rect;
		private int origSize;
		private int origStrokeWidth;
		private int origRound;
		private Paint headDownFill;

		private double centerOffset;
		private double zoom;

		private double origX;
		private double origY;

		public HeadPart(int origSize, int origStrokeWidth, int origRound, Paint headDownFill)
		{
			this.origSize = origSize;
			this.origStrokeWidth = origStrokeWidth;
			this.origRound = origRound;
			this.headDownFill = headDownFill;

			this.rect = new Rectangle(origSize, origSize, Color.TRANSPARENT);
			this.rect.setStroke(Color.BLACK);
		}

		public Node getNode()
		{
			return this.rect;
		}

		private void updatePosition()
		{
			this.rect.setX(PlantView.this.safeZoneWidth + this.origX * this.zoom - this.centerOffset);
			this.rect.setY(PlantView.this.safeZoneWidth + this.origY * this.zoom - this.centerOffset);
		}

		public void updatePosition(double x, double y, boolean headDown)
		{
			this.origX = x;
			this.origY = y;
			this.rect.setFill(headDown ? this.headDownFill : Color.TRANSPARENT);
			this.updatePosition();
		}

		public void setZoom(double zoom)
		{
			this.zoom = zoom;
			this.centerOffset = (zoom * this.origSize) / 2;
			this.rect.setWidth(zoom * origSize);
			this.rect.setHeight(zoom * origSize);
			this.rect.setStrokeWidth(zoom * origStrokeWidth);
			this.rect.setArcWidth(origRound * zoom);
			this.rect.setArcHeight(origRound * zoom);
			this.updatePosition();
		}
	}
}
