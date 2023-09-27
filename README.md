# yahboom-hat

![ ](https://cdn.shopifycdn.net/s/files/1/0066/9686/1780/products/Pi_Cooling_HAT3_700x.jpg)

## Setup

```
git clone https://github.com/tqfx/yahboom-hat.git
cd yahboom-hat
```

### Build with cmake

```
cmake -S . -B build
cmake --build build
sudo cmake --build build --target install
```

### Build with make

```
make
sudo make install
```

## Usage

```sh
$ yahboom-hat -h
Usage: yahboom-hat [options]
Options:
      --get DEV,REG     Get the value of the register
      --set DEV,REG,VAL Set the value of the register
  -c, --config FILE     Default configuration file: yahboom-hat.ini
  -v, --verbose         Display detailed log information
  -h, --help            Display available options
```

### Configuration file

```ini
i2c=/dev/i2c-0
sleep=1 # unit(s)
[led]
rgb1=0,0,0 # 0,0,0 ~ 0xFF,0xFF,0xFF
rgb2=0,0,0 # 0,0,0 ~ 0xFF,0xFF,0xFF
rgb3=0,0,0 # 0,0,0 ~ 0xFF,0xFF,0xFF
mode=disable # disable water breathing marquee rainbow colorful
speed=middle # slow middle fast
color=green # red green blue yellow purple cyan white
[fan]
mode=single # direct single graded
bound=42,60 # lower,upper or upper,lower
speed=9 # 0~9
[oled]
scroll=stop # stop left right diagleft diagright
invert=0 # bool
dimmed=0 # bool
enable=1 # bool
```

### Boot autostart

Edit the file `/etc/rc.local` and add the following content before `exit 0`:

```sh
sleep 5s
/usr/local/bin/yahboom-hat &
```

### Systemd service autostart

```sh
sudo cp yahboom-hat.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable yahboom-hat
sudo systemctl start yahboom-hat
```

## References

<http://www.yahboom.net/study/RGB_Cooling_HAT>
