## tgfs [![Build Status](https://travis-ci.org/Firemoon777/tgfs.svg?branch=master)](https://travis-ci.org/Firemoon777/tgfs)

User space filesystem for [Telegram](http://telegram.org) attachments

### API, Protocol documentation

Documentation for Telegram API is available here: http://core.telegram.org/api

Documentation for MTproto protocol is available here: http://core.telegram.org/mtproto

### Installation

#### Install libs

Debian:

```
sudo apt-get install fuse libfuse-dev libjansson-dev libreadline-dev libssl-dev libevent-dev
```
	 
Arch Linux:

```
sudo packman -S fuse2 jansson readline libevent
```

Fedora:
```
sudo dnf install fuse-devel libjansson-devel readline-devel readline-devel openssl-devel libevent-devel
```

#### Clone GitHub Repository

```
git clone --recursive https://github.com/Firemoon777/tgfs.git && cd tgfs
```

#### Build

```
./configure && make
```
	 
#### Then, install

```
sudo make install
```
	 
### Startup

You must run ```telegram-tgfs``` before the first launch of tgfs. ```telegram-tgfs``` is ```telegram-cli``` patched to show list of attachments.
After you have logged in you can use tgfs.

### Usage

Create empty dir
```
mkdir mnt
``` 
Mount tgfs
```
tgfs mnt
```
	 
### `ls -l` interpretation

Filesystem root

```
$ ls -l mnt

drwx------ 0 firemoon firemoon 0 jan 12 21:12  Alex_Ivanov
drwx-----T 0 firemoon firemoon 0 jan 12 22:22  Firemoon777
dr-x------ 0 firemoon firemoon 0 jan  1  1970  Wizard's_jokes

```

1. Permissions
	* `rwx` = user or chat (you can share anything)
	* `r-x` = public channel
	* `sticky bit` = self chat
2. atime, ctime, mtime = last seen time
	* if time is equal to zero POSIX time, it means last active time is unknown
3. display name
	 
Directories with attachments
```
$ ls  Firemoon777/
Audio  Documents  Photo  Video  Voice
```


Directory with all voice messages
```
$ ls -lh  Firemoon777/Voice/
 
-r-------- 1 firemoon firemoon 2,9K aug 31  2015 '2015-08-31 08:30:08 - 1527876055.ogg'
-r-------- 1 firemoon firemoon 4,9K sep 28  2015 '2015-09-28 07:09:20 - 3870023017.ogg
```

Sending document to user
```
cp ~/test.zip test/Documents/Firemoon777/
```

Sending any media to user (file extention is important!)
```
cp ~/000.gif mnt/Firemoon777/ # Uploaded as gif
cp ~/001.jpg mnt/Firemoon777/ # Uploaded as photo
cp ~/002.zip mnt/Firemoon777/ # Uploaded as document
```

## To do list:
- [ ] Show profile picture
- [ ] Setting profile photo with `cp`
- [ ] Removing attachments with `rm`
- [ ] FIFO-like files for chatting

## Known issues

- No dialog list reloading (if you start a new chat, you should unmount and mount tgfs again)
- No progress bars (big file will be copied in few seconds, but appears in tg within a minute)
- No multithreading
- Files with spaces in name doesn't upload

## Join 

Project chat:
https://t.me/joinchat/AAAAAApRIgKzmYvzXeh_0w
