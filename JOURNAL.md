---
title: "Multi-Cluster-Control"
github: "https://github.com/vkar9898/Multi-Cluster-Control"
description: "One PCB that controls 7 real car instrument clusters independently, from 1980s analogue gauges requiring AC, to modern CAN-bus dashboards, all running independently from a single ESP32."
created_at: "2026-10-15"
total_time: "9h 36m"
---

# October 23, 2026: Began the schematic
<!-- fabricate:entry 1116 -->

Began making the schematic for the PCB. I plan where each dashboards wire goes before starting, and make the labels along the way. Did all the power stuff (Buck's, fuses, barrel jacks, and a booster). Also wired up the ESP32, as well as the level shifters, and the 4 shift registers. It's going great so far!

![](https://fabricate.hackclub-assets.com/2b6a13975c18aa9d43834aef427fd9b585c933b98fe61feb4113c9012c19a3f5/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/IOb_KaSfofoG

**Total time spent: 1h 30m**

# October 26, 2026: Finished the schematic.
<!-- fabricate:entry 1286 -->

Finished making the schematic. Added connector footprints for the 2 CAN modules, the X27 driver, and added all other essential PCB components (transistors, resistors, and capacitors), as well as some headers.

![](https://fabricate.hackclub-assets.com/004d69aae78ce11bd65e74a40007128d423b9b10b00509ec495fcad7dad89935/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/0e1cLzEHD6jz

**Total time spent: 1h**

# October 26, 2026 (entry 2): Started making the PCB
<!-- fabricate:entry 1293 -->

Started routing the PCB. It's a bit difficult due to there being many resistors, diodes, capacitors and such on almost every wire connected to the clusters, but hopefully I can make it all work. So far I haven't started tracing, to ensure fatal mistakes don't get locked in.

![](https://fabricate.hackclub-assets.com/383641f8bfeb5a434601561879f36e09aefb56d8857dbdd773fd5ef6c4531123/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/egfRrYyQpmj3

**Total time spent: 1h 6m**

# October 27, 2026: Continued placing and routing on PCB
<!-- fabricate:entry 1335 -->

Continued placing and routing on the PCB. Finished connector 1, and the majority of the power stuff. Decided to make the bottom layer a ground pour, which'll ease my life significantly for GND connections. So far it looks a bit cluttered, but there really isn't a much better way of doing it without relying quite heavily on vias.

![](https://fabricate.hackclub-assets.com/0cf751c092585d822afe01ee483999dbee981ed796c0ec3d327dcb795b31bfe0/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/pn_k8kwHLIcg

**Total time spent: 1h 30m**

# October 28, 2026: More PCB routing
<!-- fabricate:entry 1480 -->

Continued routing the PCB, as usual. Started the CN2 connector a fixed up some tracing issues with CN1 (things like not enough clearance, duplicate vias and such).

![](https://fabricate.hackclub-assets.com/d985f08fc1a7cbc2746aab263fe3a675205c27f315618fc1f94d914f42f678f1/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/eUwztKtLVqtG

**Total time spent: 30m**

# October 29, 2026: Almost finished with the PCB
<!-- fabricate:entry 1528 -->

Almost finished with the PCB. Finished off CN2, and did the bulk of CN3, while ensuring that all lines get proper power with a thick enough trace. This was a little bit of a challenge on CN2, but I managed to make it work via the long route along the right of the PCB. It's starting to come together now, and I am becoming increasingly happier as I notice that there are less components to place.

![](https://fabricate.hackclub-assets.com/28104b60f43d8c4b8664d6c0a92a6eb1dd495b9f48a808fb8041637cc828ff54/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/g2Dk5GwVGELh

**Total time spent: 1h 30m**

# October 30, 2026: Finished the PCB
<!-- fabricate:entry 1643 -->

Finally completed the PCB in a long 2.5 hour session. Did CN3, CN4, and the U6-8 modules sitting underneath the ESP32, making sure no traces sit under the antenna. Also added some cool art of the W126 front, Suzuki logo (car is too niche to have any royalty free blueprints), GT86, and the F10. It looks great! The hardest part of this is definitely grounding everything, there were so many islands left that needed grounds...

![](https://fabricate.hackclub-assets.com/42419233b4bb060921c3e2494d8d7820fe90a1f91ea9fca166e71805f94989b3/image-png/image.png)

Timelapse: https://lapse.hackclub.com/timelapse/QanxuUaG0hgY

**Total time spent: 2h 30m**
