Follow https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html

when installing the idf tools install the 5.0.2 offline installer.

Add the espressif idf extension to vscode

Press control+shift+p and search esp-idf: welcome

On this page select configure extension and select use existing extension.

Use the default settings.

Next clone the repository to your system

In VScode: 
- Click the gear box on the bottom left of the VScode
  - Select "Example Connection Configuration"
  - Verify that "connect using WiFi interface" and "Provide wifi connect commands" are checked 
  - Type in the name of your wifi or hotspot 
  - Type in the password 
  - Click "save" NOTE: if save is in white then the changes haven't been saved. 
  
- On the bottom toolbar, then click on the flame icon to build, flash, and monitor
  - Important: Press and hold onto the IOO button on the ESP module 

If successful, the ESP module is connected to wifi and flashing the blue LED. 
