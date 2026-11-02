# Orange Pi Zero 2 W notes

## Connect to Orange Pi Zero 2 W via SSH

First of all you need to be connected to the same network as the Orange Pi Zero 2 W, this can be done with nmcli or with the GUI network manager.

```bash
nmcli device wifi list
nmcli device wifi connect <SSID> password <PASSWORD>
```

Once the network is added once it can be connected or disconnected with:

```bash
nmcli connection up <SSID>
nmcli connection down <SSID>
```

Then you can connect to the Orange Pi Zero 2 W via SSH with:

```
ssh orangepi@equipo12
cont12
```

