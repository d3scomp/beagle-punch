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

import jssc.*;

import bpv.plant.*;
import bpv.serial.*;

public class Visualizer extends Application
{

	private String serialFileName = "/dev/ttyUSB0";
	private String punchFileName = null;
	private String logFileName = null;

	private SerialCom serialPort;
	private PlantState plant;
	private Timeline updateTimeline;

	// main part
	private PlantView plantView;

	// labels on the bottom status bar
	private Label connectionStatusLabel;
	private Label positionLabel;
	private Label velocityLabel;
	private Label statusLabel;

	// right panel
	private PunchView punchView;
	private InitPosView initposView;

	private Stage primaryStage;

	private Node createPunchPanel()
	{
		BorderPane pane = new BorderPane();

		this.punchView = new PunchView(this.plant.getPlannedPunches());
		pane.setCenter(this.punchView);

		this.initposView = new InitPosView(this.plant, this.primaryStage);
		pane.setBottom(this.initposView);

		return pane;
	}

	private Node createStatusBar()
	{
		HBox bar = new HBox();
		bar.setPadding(new Insets(5, 5, 5, 5));
		bar.setSpacing(5);
		bar.setStyle("-fx-background-color: #999999;");
	
		this.connectionStatusLabel = new Label();
		this.positionLabel = new Label();
		this.velocityLabel = new Label();
		this.statusLabel = new Label();
		Separator s = new Separator();
		s.setOrientation(Orientation.VERTICAL);
		Separator s2 = new Separator();
		s2.setOrientation(Orientation.VERTICAL);
		Separator s3 = new Separator();
		s3.setOrientation(Orientation.VERTICAL);
		bar.getChildren().addAll(this.connectionStatusLabel, s,this.statusLabel, s2, this.positionLabel, s3, this.velocityLabel);
	
		return bar;
	}

	private Node createPlantView()
	{
		BorderPane pane = new BorderPane();
		this.plantView = new PlantView();
		pane.setCenter(this.plantView);

		Slider slider = new Slider();
		slider.setMin(0);
		slider.setMax(100);
		slider.setValue(50);
		slider.setShowTickLabels(false);
		slider.setShowTickMarks(true);
		slider.setBlockIncrement(10);
		pane.setBottom(slider);

		slider.valueProperty().addListener(new ChangeListener<Number>()
		{
			public void changed(ObservableValue<? extends Number> ov, Number oldValue, Number newValue)
			{
				double position = (newValue.doubleValue() - 50) / 20;
				double zoom = Math.pow(Math.abs(position), Math.log(10) / Math.log(5));
				if (position > 0)
				{
					zoom += 1;
				}
				else
				{
					zoom = 1 / (zoom + 1);
				}

				Visualizer.this.plantView.setZoom(zoom);
				PlantState plant = Visualizer.this.plant;
				if (plant != null)
				{
					Visualizer.this.plantView.redraw(plant);
				}
			}
		});
		
		return pane;
	}

	private void printUsage()
	{
		System.err.println("Usage:");
		System.err.println("./run [-h] [-i <punch file>] [-o <log file>] [-s <serial port>]");
		System.err.println("-i <punch file> - file with planned punches, planned punches are displayed in the work area; on each line of the file a pair of coordinates separated by a c      omma is expected");
		System.err.println("-o <log file> - file used for logging of errors and punches");
		System.err.println("-s <serial port> - serial port used for communication with the plant, default is /dev/ttyUSB0");
		System.err.println("-h - print this help and immediately exits the program");
		System.err.println("Available serial ports:");
		for (String name : SerialPortList.getPortNames())
		{
			System.err.println(name);
		}

	}

	private boolean parseArgs(String[] args)
	{
		for (int i = 0; i < args.length; ++i)
		{
			String option = args[i];
			if (option.equals("-h"))
			{
				printUsage();
				return false;
			}
			else if (i + 1 < args.length)
			{
				String param = args[i + 1];
				++i;
				if (option.equals("-i"))
				{
					punchFileName = param;
				}
				else if (option.equals("-o"))
				{
					logFileName = param;
				}
				else if (option.equals("-s"))
				{
					serialFileName = param;
				}
				else
				{
					printUsage();
					return false;
				}
			}
			else
			{
				printUsage();
				return false;
			}
		}

		return true;
	}

	private void createPlant() throws IOException, SerialPortException
	{
		this.serialPort = new SerialCom(serialFileName);
		this.plant = new PlantState(this.serialPort);
		if (punchFileName != null)
		{
			plant.loadPlannedPunches(punchFileName);
		}

		if (logFileName != null)
		{
			plant.setLogFile(logFileName);
		}
	}

	@Override
	public void stop()
	{
		if (this.updateTimeline != null)
		{
			this.updateTimeline.stop();
		}

		try
		{
			if (this.plant != null)
			{
				this.plant.close();
			}

			if (this.serialPort != null)
			{
				this.serialPort.close();
			}
		}
		catch (IOException e)
		{
		}
	}

	private void updateStatusBar()
	{
		// connectionStatusLabel, positionLabel, statusLabel
		this.connectionStatusLabel.setText(this.plant.previousUpdateCompleted() ? "Connected" : "NOT CONNECTED");
		this.positionLabel.setText(String.format("position - x: %.2f mm, y: %.2f mm", this.plant.getXPos(), this.plant.getYPos()));
		this.velocityLabel.setText(String.format("velocity - x: %.4f m/s, y: %.4f m/s", this.plant.getXVelocity(), this.plant.getYVelocity()));
		this.statusLabel.setText(this.plant.isOK() ? "status: OK" : "status: FAILED");
	}

	private void update()
	{
		// update view according to the current known state
		this.plantView.update(this.plant);
		this.punchView.update(this.plant);
		this.initposView.update();
		this.updateStatusBar();
		
		// initiate PlantState update
		this.plant.update();
	}

	@Override
	public void start(Stage primaryStage) throws Exception
	{
		this.primaryStage = primaryStage;

		List<String> params = this.getParameters().getRaw();
		String[] args = new String[params.size()];
		args = params.toArray(args);
		if (!this.parseArgs(args))
		{
			Platform.exit();
			return;
		}

		this.createPlant();

		BorderPane root = new BorderPane();
		root.setBottom(this.createStatusBar());
		root.setRight(this.createPunchPanel());
		root.setCenter(this.createPlantView());

		Scene scene = new Scene(root, 800, 500);

		primaryStage.setTitle("Visualizer");
		primaryStage.setScene(scene);
		primaryStage.show();

		this.update();

		// periodic update
		this.updateTimeline = new Timeline(new KeyFrame(Duration.millis(50), new EventHandler<ActionEvent>()
		{
			@Override
			public void handle(ActionEvent event)
			{
				Visualizer.this.update();
			}
		}));
		this.updateTimeline.setCycleCount(Timeline.INDEFINITE);
		this.updateTimeline.play();
	}

	public static void main(String[] args) {
		launch(args);
	}
}
