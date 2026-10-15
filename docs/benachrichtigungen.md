# Benachrichtigungen

ServiceTracker meldet Ereignisse, die eine Abteilung betreffen, als Windows-Benachrichtigung
und mit einem roten Punkt am Taskleisten-Symbol – ähnlich wie Teams bei neuen Chat-Nachrichten.

## Für Anwender

| Was | Verhalten |
|---|---|
| Windows-Benachrichtigung | erscheint, sobald etwas Neues anliegt, und beim Programmstart, wenn bereits etwas ansteht |
| Anzeigedauer | bleibt stehen, bis sie angeklickt oder mit **Schließen** weggeklickt wird (abschaltbar) |
| Klick / **Öffnen** | holt ServiceTracker in den Vordergrund und öffnet das passende Fenster (z. B. „Bestellungen“) |
| Neuer Stand | ersetzt die vorige Benachrichtigung derselben Art (keine Stapel) |
| Nichts mehr offen | die Benachrichtigung verschwindet automatisch (auch aus dem Benachrichtigungscenter) |
| Programm im Vordergrund | keine Windows-Benachrichtigung, die Meldung erscheint unten rechts im Programm |
| Roter Punkt in der Taskleiste | solange mindestens ein Eintrag ansteht (z. B. versandbereite Bestellungen) |
| Symbol im Infobereich | erscheint mit der ersten Benachrichtigung; Tooltip zeigt den aktuellen Stand, Klick öffnet ServiceTracker |
| Eigene Änderungen | lösen keine Benachrichtigung aus |

Menü **Einstellungen → Benachrichtigungen** (nur sichtbar, wenn der Benutzer Benachrichtigungen erhält):

- *Windows-Benachrichtigungen* aus → Meldungen nur noch im Programm.
- *Benachrichtigung bleibt bis zum Schließen* aus → Windows blendet sie nach einigen Sekunden
  aus, sie bleibt im Benachrichtigungscenter.
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

Technik unter Windows:

- Toast-Benachrichtigung über C++/WinRT (`ToastNotificationManager`). Die App-ID
  `ServiceTracker.Desktop` wird bei jedem Start unter
  `HKCU\Software\Classes\AppUserModelId\ServiceTracker.Desktop` (Name + `icon.png`)
  eingetragen – keine Adminrechte, keine Verknüpfung, keine Installer-Änderung nötig.
- „Bleibt bis zum Schließen“ = Szenario `reminder` mit den Schaltflächen *Öffnen* und
  *Schließen*. Tag = Kanal-ID, dadurch ersetzt ein neuer Stand den alten Toast.
- Klicks kommen auf einem Hintergrund-Thread an und werden per `PostMessage` an den
  UI-Thread übergeben.
- Rückfallebene, falls Toasts nicht verfügbar sind: Infobereich-Sprechblase
  (`Shell_NotifyIcon`, verschwindet nach wenigen Sekunden).
- Roter Punkt: `ITaskbarList3::SetOverlayIcon`. Im Smoke-Test ersetzt
`RecordingNotifier` die Windows-Anbindung und prüft Anzeige, Klick und roten Punkt.
