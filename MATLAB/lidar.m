%Initialization
rosshutdown
setenv('ROS_MASTER_URI','http://192.168.0.124:11311');  
setenv('ROS_IP', '192.168.0.112');
rosinit(getenv('ROS_MASTER_URI'));

% Parameters
sub = rossubscriber('/scan');
maxLidarRange = 6;
mapResolution = 20;
slamAlg = lidarSLAM(mapResolution, maxLidarRange);
slamAlg.LoopClosureThreshold = 200;
slamAlg.LoopClosureSearchRadius = 3;
scanLimit = 2e7;
controlRate = robotics.Rate(5);
j = 0;

for i = 1:scanLimit
    % Process Raw Scans
    raw_scan = lidarScan(receive(sub));
    scan = transformScan(raw_scan,[0,0,deg2rad(180)]);
    subplot(1,2,1);
    plot(scan);
    title('Close Range Plot')

    % Draw to scale robots boundaries
    rectangle('Position',[((-0.28/2)+(0.07)) -0.16/2 0.28 0.16])

    % Close Range Plot
    axis([-0.75 0.75 -0.75 0.75]))
    j = j + 1;
    if rem(j, 10) == 0
        addScan(slamAlg, scan);
        subplot(1, 2, 2);
        show(slamAlg);
    end
end