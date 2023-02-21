# Lab_3_FAT_File_System
School assignment for a course in Operating Systems for creating a full simulation of a FAT file system, including formating and handling of memory on a simulated disk defined by FAT16 documentation.

The assignment started off by telling what the expected general functionality of the filesystem should be and included the "disk" and "shell" files as well as an entry point to properly create a simulated environment to interact with.

All self-made code is in the "fs" files and contains all the expected functionality of the filesystem. The program is fully error checked for bad writes, illegal operations/file namings, tries to access un-initialized memory blocks on the disk, and more.

This repo also includes the report that was neccassary for the assignment and goes through the most essential parts of the project as well as the thought process behind its implementation.

## Running the Code
The code builds using a Make. To run the program, open the folder in the terminal and type "make". If you want to change the compiler, change the "CC" variable to any other C++ compiler. Make sure to run "make all" after any changes to fully clean and recompile the program.

Make sure to run the "format" command if it is the first time running the program as this will properly initialize a file on the system that simulates the memory disk. 
