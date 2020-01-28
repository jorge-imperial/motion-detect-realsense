# Motion detector

## Overview

This project demonstrates how to detect motion in 3D cloud points, using the Intel Realsense library, and how to store and display detected objects geometry in MongoDB, using either Atlas or a local database 
 
 
This is implemented in two parts: a client application running on a single board computer and mobile application that receives updates.

The client application  reads frames from the camera. It acquires a point cloud from a frame  ( a set  of 3D vertices at a given time), and compares them to subsequent frames.
If a frame is sufficiently different, this difference is determined to be an object that has entered the scene.  The object is uploaded to a database.  The mobile application receives the changes in scene and displays them.


1 - Every predetermined period of time a whole frame is uploaded for reference to the database.
2 - Objects entering the region of interest are uploaded when they are detected.
3 - After a predetermined period of time, an object that has entered the scene stops being reported.
4 - Another application monitors changes and reports them to 


The client application is written in C++ and uses a MongoDB database to store information.
The monitoring application is written using Stitch.


