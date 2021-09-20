# Planto

Ein automatisiertes Mini-Gewächshaus für Kids zum Lernen und Wachsen.

-main.cpp: Enthält alle wichtigen Funktionen des Plantos bezüglich der Steuerung der Sensoren und Integation des Menüs, sowie Aufnau & Kommunikation mit dem Webserver 
-planto_menu.h: Ausgelagerte Klasse, in der alle nötigen Definitionen und Methoden für das Menu enthalten sind
-bootstrap.h: Klasse zur Verbindung mit dem lokalen WIFi ohne, dass das Passwort manuel über den COode aktualisiert werden muss
-fan.cpp: Klasse, die die Methoden des Fans enthält 
-fan.h: Klasse zur Nutzung der Fan.cpp Klasse

Der Planto hat damit folgende Funktionen: 

-Temperatur & Luftfeuchtigkeit könenn angezeigt werden
-Die Helligkeit kann gemessen werden
-Die LED kann an und ausgeschafltet werden, auch über den Webserver
-Der Ventilator kann an und ausgeschaltet werden, auch über den Webserver 
-Verbindung mit dem Webserver über lokale IP ist möglich 
-Automatisches Booststrapping, wenn sich der Planto in einem unbekannten WIFi befindet 
