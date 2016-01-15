```
              __________              ___.     _________.__           .__  .__  
       ,--.!, \______   \ ____   _____\_ |__  /   _____/|  |__   ____ |  | |  |  
    __/   -*-  |    |  _//  _ \ /     \| __ \ \_____  \ |  |  \_/ __ \|  | |  |  
  ,d08b.  '|`  |    |   (  <_> )  Y Y  \ \_\ \/        \|   Y  \  ___/|  |_|  |__
  0088MM       |______  /\____/|__|_|  /___  /_______  /|___|  /\___  >____/____/
  `9MMP'              \/             \/    \/        \/      \/     \/           
              v1.0 by Annihil & Dziter
```

The purpose of this project was to build a basic shell in C/C++ programming languages.


## Requirements
- g++
- make

## Installation
```sh
make
./sh
```

## Features
- Execution of basic commands like "cd", "ls", "touch" or "mkdir"
- Execution of commands as foreground or background process groups. For example, "ls -l", "./myprog&", etc
- Redirection of commands standard input and standard output. For example, " echo "hello" > x.txt ", "sort < in.txt", "ls -l > myfile", etc
- Cascade pipelines commands such as "ls | sort | uniq | wc"
- Send SIGHUP, SIGINT (CTRL+C), and SIGQUIT (CTRL+\) signals to the right process groups
- List background processes using the command "jobs"
- Exit BomberShell by typing "exit" (which kills all the processes) or press "CTRL+D"

# Additional features
- Run process in background by typing "&"
- Put last executed process in background using "bg"
- Put last executed process in foreground using "fg"
- Kill a previously launched job by its job id using "killjob &lt;job_id&gt;"
- Display the processes currently in background with pid, pgid and program's name by typing "jobs"

You can find the description of these extra features by typing "help" in BombShell