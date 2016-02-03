# Oracle Java 8 is required because of the JavaFX dependency
JAVAC=/usr/java/latest/bin/javac

CLASSPATH=-cp ./jssc.jar:./javax.json-1.0.4.jar:.

SRCS=\
	bpv/plant/PlantState.java \
	bpv/plant/Punch.java \
	bpv/plant/SessionId.java \
	bpv/serial/SerialComEventListener.java \
	bpv/serial/SerialCom.java \
	bpv/serial/SerialComMessage.java \
	bpv/view/InitPosView.java \
	bpv/view/PlantView.java \
	bpv/view/PunchView.java \
	bpv/view/Visualizer.java

.PHONY: compile

compile:
	$(JAVAC) $(CLASSPATH) $(SRCS)

clean:
	rm -rf bpv/plant/*.class bpv/serial/*.class bpv/view/*.class

