# CG2111A Engineering Principles and Practice II - Alex


Alex is a remotely-operated robot, designed to navigate an unknown maze and locate and identify surviors, without prior mapping or location information.

The robot operates with a Raspberry Pi 4 Model B, Arduino Mega 2560, RPLidar A1, TCS3200 Colour Sensors and a standard HC-SR04 ultrasonic sensor. For the exact system architecture, refer to the following section.

## System Architecture
![System Architecture UML Diagram](/meta/System%20Architecture.jpg)
## Remarks
### SLAM with MATLAB

1. To visualize the map generated by Hector SLAM, MATLAB's ROS Toolbox was used due to its ease of usage, extensive documentation and versatility, as compared to the default RViz software offered by ROS which proved tedious to configure.
   
2. A live point cloud plot was used to visualize the immediate surroundings of the robot, with a to-scale rectanlge super-imposed on the point cloud plot. This allowed bump-free navigation, and allowed the pilot to orientate the robot without the need for an IMU sensor. 

![Point Cloud & SLAM Running on MATLAB](/meta/SLAM.jpg)

### `getch()` using `<termios.h>`

A _\<WASD\>_ control system was adopted over the provided default manual drive in order to save time. Continous movement will be discretized into 5 cm/10 degree movements to achieve the intended effect.

Therefore it was necessary to disable normal terminal operation and allow the streaming of asynchronous keypresses from the keyboard to the Pi. Since `tls-alex-client` ran on a MacBook, `getch()` needed to be implemented manually. Do note, `getch()` is defined for the Windows console in `<conio.h>`

```C++
char getch() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) != 1) c = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}
```

Since a constant stream of movement commands arrive at the Arduino, the 

### ROS Networking Initialization

Since SLAM is off-loaded from the Pi to the a host computer running MATLAB, a ROS Publisher Node must be initialized on the Raspberry Pi. For faster setup, append the following lines to the `.bashrc` of the Pi.

```sh
export ROS_MASTER_URI="http://<PI_IP_ADDR>:11311"
export ROS_IP="<PI_IP_ADDR>"
```

This will configure the ROS publisher to publish the LiDAR data on `localhost:11311`. The [SLAM program](/MATLAB/lidar.m) running on MATLAB will connect to this IP address to receive LiDAR point cloud data.

### k-NN Colour Classification
A kNN classifier was used to classify the unknown objects within the maze. Since there were only 3 distinct colours (red, green & white), we deemed a simple k-NN would be sufficiently accurate to predict obstacle colours. However, the k-NN classifier can be easily swapped out from the code for any other classifer appropriate for other use cases.

The optimal k-value was empirically determined to be 5, with diminishing accuracies for k-values lower or higher than 5, due to overfitting and overgeneralisation respectively.

```C++
TColour kNNClassifier(RGB col, int dataset[MAX_PARTITION][MAX_PARAM]) {
    // Empirically Determined k-Value
    const int k = 5;

    // Calculate Euclidean Error between all data points
    TColour predictedCol = RED;
    unsigned long minDist = 1e9;
    ErrorData errors[MAX_PARTITION];
    for (int i = 0; i < MAX_PARTITION; i++) {
        RGB col2 = {dataset[i][0], dataset[i][1], dataset[i][2]};
        unsigned long error = euclidError(col, col2);
        errors[i] = {euclidError(col, col2), dataset[i][3]};
    }

    sort(errors, MAX_PARTITION);

    // Populating histogram of k nearest colours
    int tiebreaker[3] = {0, 0, 0};
    int votes = 0;
    for (long i = 0; i < k; i++) {
        tiebreaker[errors[i].colClass]++;
    }

    // Finding the mode
    for (int i = RED; i <= WHITE; i++) {
        if (tiebreaker[i] > votes) {
            votes = tiebreaker[i];
            predictedCol = (TColour) i;
        }
    }

    return predictedCol;
}
```

## Usage
**Pre-requisites**\
TLS Certificates and Keys are not published on GitHub. Create self-signed certificates and keys, and ensure that the certificate and key naming conventions match the conventions specified in the [server](/TLS%20Server/tls-alex-server.cpp) and [client](/TLS%20Client/tls-alex-client.cpp) programs, or modify the programs accordingly.

**Navigation**
1. Copy the TLS Server folder to the Raspberry Pi.
2. Copy the TLS Client folder to the host laptop.
3. Compile both the server and client programs using the `make` command provided in the makefile.
4. Connect Arduino Mega to the Pi, and execute the server program on Pi over SSH.
5. Execute client program on host computer. Check basic movements.

The key-bindings to operate the robot are shown below:

| Keypress (case-insensitve)  | Command |
| ------------- | ------------- |
| `W`  | Move Forwards  |
| `A`  | Turn Left  |
| `S`  | Move Backwards  |
| `D`  | Turn Right  |
| `T`  | Toggle Between Auto and Manual  |
| `P`  | Force Halt  |
| `G`  | Get Status  |
| `C`  | Clear Counters  |


\
The toggle exists as a precaution, in order to toggle between auto and manual driving, in case more granular control is required. Auto refers to `<WASD>` driving of the robot, where continuous movement is discretized into small movements, and manual refers to the slower version of entering `<direction> <speed> <distance>` into the client. 

**Mapping and Localisation**

Since SLAM is off-loaded to a host computer running MATLAB, a ROS Publisher node must be initialized on the Pi using the following command (specific to Slamtec Lidars):

```sh
roslaunch rplidar_ros rplidar.launch
```

After a health status of 0 is received from the LiDAR, the SLAM on MATLAB is ready to run, and receive the point-cloud information.
