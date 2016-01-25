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

public class InitPosView extends VBox
{
	private PlantState plant;

	private Label positionLabel;

	private Stage dialog;

	public InitPosView(PlantState plant, Stage primaryStage)
	{
		this.plant = plant;

		Label caption = new Label("Initial position:");
		this.positionLabel = new Label();

		this.dialog = this.createWindow(primaryStage);

		Button btn = new Button();
		btn.setText("Change");
		btn.setOnAction(new EventHandler<ActionEvent>()
		{
			@Override
			public void handle(ActionEvent event)
			{
				InitPosView.this.dialog.show();
			}
		});
		this.getChildren().addAll(caption, this.positionLabel, btn);

	}

	private Stage createWindow(Stage primaryStage)
	{
		BorderPane pane = new BorderPane();
		Scene scene = new Scene(pane);
		final Stage s = new Stage();//StageStyle.TRANSPARENT); // UNDECORATED
		s.initModality(Modality.WINDOW_MODAL);
		s.initOwner(primaryStage);
		s.setScene(scene);
		s.setTitle("Initial position");

		GridPane main = new GridPane();
		main.setAlignment(Pos.CENTER);
		main.setHgap(10);
		main.setVgap(10);
		main.setPadding(new Insets(25, 25, 25, 25));
		pane.setCenter(main);

		RadioButton rnd = new RadioButton("Random");
		main.add(rnd, 1, 1);

		Label xLabel = new Label("x:");
		Label yLabel = new Label("y:");
		TextField xPos = new TextField();
		TextField yPos = new TextField();

		main.add(xLabel, 0, 2);
		main.add(xPos, 1, 2);
		main.add(yLabel, 0, 3);
		main.add(yPos, 1, 3);

		rnd.selectedProperty().addListener(new ChangeListener<Boolean>()
		{
			public void changed(ObservableValue<? extends Boolean> ov, Boolean oldValue, Boolean newValue)
			{
				xPos.setDisable(newValue);
				yPos.setDisable(newValue);
			}
		});

		Button ok = ButtonBuilder.create().text("OK").defaultButton(true).onAction(new EventHandler<ActionEvent>()
		{
			@Override
			public void handle(ActionEvent e)
			{	
				if (rnd.selectedProperty().getValue())
				{
					InitPosView.this.plant.setInitPosRandom();
				}
				else
				{
					try
					{
						InitPosView.this.plant.setInitPos(Double.parseDouble(xPos.getText()), Double.parseDouble(yPos.getText()));
					}
					catch (NumberFormatException ex)
					{
						System.err.println(ex);
						return;
					}
				}
				dialog.close();
			}
		}).build();
		Button cancel = ButtonBuilder.create().text("Cancel").cancelButton(true).onAction(new EventHandler<ActionEvent>()
		{
			@Override
			public void handle(ActionEvent e)
			{
				dialog.close();
			}
		}).build();

		HBox btns = new HBox();
		btns.setSpacing(50);
		btns.setPadding(new Insets(5, 0, 10, 0));
		btns.setAlignment(Pos.CENTER);
		btns.getChildren().addAll(ok, cancel);
		pane.setBottom(btns);

		return s;
	}

	public void update()
	{
		if (this.plant.isInitPosRandom())
		{
			this.positionLabel.setText("RANDOM");
		}
		else
		{
			this.positionLabel.setText(String.format("x: %.2f, y: %.2f", this.plant.getXInitPos(), this.plant.getYInitPos()));
		}
	}
}
