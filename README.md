# FAT Filesystem simulation
This project was a larger school assignment for a course in Operating Systems for creating an extensive but still basic simulation of a Unix-based FAT filesystem, including formating and handling of memory on a simulated hard drive, defined by FAT16 documentation. 

All self-made code are in the "fs" files and contains all the expected functionality of the filesystem. The program has extensive error checking for bad writes, illegal operations/file namings, attempts to access un-initialized memory blocks on the disk, and more. The filesystem is navigated through a simulated terminal and uses common filesystem commands and syntax for all of its functionality ('cd', 'mkdir', 'rm', etc).

This repo also includes a written report that was neccassary for the assignment and goes through the most essential parts of the project as well as the thought process behind the implementation.

## Running the Code
The code builds using a Make. To run the program, open the folder as your working directory in your terminal and type "make". If you want to change the compiler, change the "CC" variable in the makefile to any other C++ compiler. Make sure to run "make all" after any changes to fully clean and recompile the program.

Make sure to run the "format" command if it is the first time running the program as this will properly initialize a file on the system that simulates the hard drive. 
