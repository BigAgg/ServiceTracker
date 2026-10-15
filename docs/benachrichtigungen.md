# Benachrichtigungen

ServiceTracker meldet Ereignisse, die eine Abteilung betreffen, als Windows-Benachrichtigung
und mit einem roten Punkt am Taskleisten-Symbol – ähnlich wie Teams bei neuen Chat-Nachrichten.

## Für Anwender

| Was | Verhalten |
|---|---|
| Windows-Benachrichtigung | erscheint, sobald etwas Neues anliegt, und beim Programmstart, wenn bereits etwas ansteht |
| Klick auf die Benachrichtigung | holt ServiceTracker in den Vordergrund und öffnet das passende Fenster (z. B. „Bestellungen“) |
| Programm im Vordergrund | keine Windows-Benachrichtigung, die Meldung erscheint unten rechts im Programm |
| Roter Punkt in der Taskleiste | solange mindestens ein Eintrag ansteht (z. B. versandbereite Bestellungen) |
| Symbol im Infobereich | erscheint mit der ersten Benachrichtigung; Tooltip zeigt den aktuellen Stand, Klick öffnet ServiceTracker |
| Eigene Änderungen | lösen keine Benachrichtigung aus |

Menü **Einstellungen → Benachrichtigungen** (nur sichtbar, wenn der Benutzer Benachrichtigungen erhält):

- *Windows-Benachrichtigungen* aus → Meldungen nur noch im Programm.
- *Roter Punkt in der Taskleiste* an/aus.
- Pro Art der Benachrichtigung ein Schalter und ein **Testen**-Button.

Hinweise:

- Der rote Punkt (Overlay-Symbol) wird von Windows nur bei großen Taskleisten-Schaltflächen
  angezeigt.
- „Nicht stören“ / Fokus-Assistent von Windows unterdrückt die Benachrichtigungen, der rote
  Punkt bleibt sichtbar.
- Benachrichtigungen gibt es nur, solange ServiceTracker läuft (auch minimiert).

## Vorhandene Benachrichtigungen

| Kanal-ID | Empfänger | Titel | Text | Fenster |
|---|---|---|---|---|
| `orders.prepared` | `SALES` | ServiceTracker - Bestellungen | Es sind *n* Bestellungen versandbereit | Bestellungen |

## Für Entwickler: neue Benachrichtigung einbauen

Die Logik (was ist neu, wann wird gemeldet, Stummschalten) liegt testbar in
`core/include/st/alerts/alert_center.h`, die Windows-Anbindung in
`app/src/framework/desktop_notifier.*`, das Zusammenspiel in `app/src/framework/alerts.*`.
Ein Modul braucht nur zwei Aufrufe:

```cpp
// init(): Kanal anmelden (nur wenn der Benutzer zur Zielgruppe gehört)
alerts::Channel channel;
channel.id = "devices.received";               // steht in preferences.json – nie umbenennen
channel.label = "Neue Geräte im Wareneingang"; // Einstellungen
channel.title = "ServiceTracker - Wareneingang";
channel.mode = alerts::Mode::PerItem;          // eine Meldung pro neuem Eintrag
channel.itemText = [](const alerts::Item &item) { return "Neuer Reader im Wareneingang " + item.label; };
channel.summaryText = [](std::size_t n) { return alerts::countSentence(n, "Reader", "Reader", "im Wareneingang"); };
channel.target = "Reader Control";             // Fenster, das ein Klick öffnet
channel.audience = {"TECHSUPPORT"};            // leer = alle Benutzer
ctx.alerts.addChannel(ctx, channel);

// bei jeder Datenänderung (z. B. nach dem Polling in update()): alle aktuell offenen Einträge melden
ctx.alerts.report("devices.received", {{"0601-1234", "0601-1234"}, ...});

// eigene Änderung nicht melden
ctx.alerts.markSeen("devices.received", "0601-1234");
```

Regeln der `AlertCenter`:

- **Summary**: eine Meldung mit der Gesamtzahl, sobald ein neuer Eintrag dazukommt. Mehrere
  Änderungen vor der Anzeige ergeben nur eine Meldung mit der neuesten Zahl.
- **PerItem**: eine Meldung pro neuem Eintrag; mehr als `maxItemNotifications` neue Einträge
  auf einmal werden zu einer Zusammenfassung (`summaryText`).
- Erste Meldung nach dem Start: alle vorhandenen Einträge gelten als neu
  (`notifyOnStart = false` schaltet das ab).
- Verschwundene Einträge lösen nichts aus; kommt ein Eintrag zurück, wird er wieder gemeldet.
- Der rote Punkt zeigt die Summe aller nicht stummgeschalteten Kanäle.

Technik unter Windows: Infobereich-Symbol mit Sprechblase (`Shell_NotifyIcon`, wird von
Windows 10/11 als normale Benachrichtigung angezeigt – keine Verknüpfung oder App-ID nötig)
und `ITaskbarList3::SetOverlayIcon` für den roten Punkt. Im Smoke-Test ersetzt
`RecordingNotifier` die Windows-Anbindung und prüft Anzeige, Klick und roten Punkt.
