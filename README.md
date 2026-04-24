# Vital Sense (EmbIoT)

## Introduction

This is a vital sense application for [PSoC 6 AI Evaluation Kit](https://www.infineon.com/evaluation-board/CY8CKIT-062S2-AI)

The application have two parts:
- main.c: Read onboard radar data and stream raw data to the host machine.
- host/*: Run on host machine that accepts the raw data and visualize the analysis.

## Requirements

**We've only tested this on Windows 11 and MacOS**
- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.7 or later (tested with v3.7)
- Python3 (tested with Python 3.14.4)
- Visual Studio Code
- 2 usb-c cable

## Setup

1. Download and install [ModusToolbox Setup](https://www.infineon.com/design-resources/development-tools/sdk/modustoolbox-software)

2. Open ModusToolbox Setup. (On windows, you can search ModusToolboxSetup in windows search bar. On MacOS, use command-space to search)

3. You will need to register and log-in (top left corder of the app)

4. Once logged in, select "Default ModusToolbox Installation" and install.

5. Open the dashboard app (Search dashboard in windows/macos searchbar. the app is called ModusToolbox Dashboard on windows, and dashboard.app on MacOS)

6. At the top-right corner, select visual studio code as IDE

7. Click "Launch Project Creator"
![Step 2-3](images/step_create.png)

8. If the board is connected to the PC, you should see "\*\*Detected devices\*\*". Select that. Otherwise, select PSOC 6 BSPs, then CY8CKIT-062S2-AI. Click "Next".
![Step 4](images/step_select_board.png)

9. Select "Getting Started", then "Empty App". Change the application name or keep it as is. Change path at the top if needed. Then, click "Create"
![Step 5](images/step_select_app.png)

10. Clone this repo in a temporary folder
```
git clone git@github.com:Jazzhsu/EmbIoT_VitalSense.git
```

11. Copy everything in repo into the empty app folder you just created in step 9

12. Open VSCode. File -> Open File. Go to the empty app folder, select `mtb-example-empty-app.code-workspace`

13. Click "Open Workspace" at the bottom right of the file.
![Step 9](images/step_open_ws.png)

14. Terminal -> Run Task..., Select "Tool: Library Manager". Continue without scanning.

15. Verify: you should see "sensor-xensiv-bgt60trxx" as the last item in the library list. Click "Update"

16. Terminal -> Run Task..., Select "Build". Wait for the build.

17. Connect both USB-C port on the board to your PC.

18. Terminal -> Run Task..., Select "Program". Wait for the board programming to be complete.

19. Verify: you can go to the VSCode NRFConnect extension. You should see device "3150C5A012D2400". Connect to the VCOM0 of that device. You should see (You may need to reset or reprogram the device to see the output):
```
****************** Vital Sense Application ****************** 

0:000 USBD_Start
BGT60TRXX setup complete
```

20. Open a terminal, goto the project folder and cd into `host/`
```
cd host
```

21. Install required python packages
```
pip3 install -r requirements.txt
```

22. Find the usb port name:

MacOS
```
ls /dev/tty.usbmodem2439*
```

Windows
```
TODO
```

In my case the name is `/dev/tty.usbmodem24391`

23. Start host application (replace the usb port name if necessary):
```
python3 host.py -s /dev/tty.usbmodem24391
```

24. You should now see a plot popped up. (You will need to wait a few seconds for the graph to be propagated.
![Step19](images/graph1.png) 


25. Face the board towards your chest (keep ~50cm distance from your chest) and keep it steady. Or place it on the table so that the board is facing your chest.

In graph 1 you should see a peak except the first 5 bins like this. That peak is you.
Graph 2 & 3 is your chest displacement for breath and heartbeat respectively.
![Step20](images/graph2.png)

