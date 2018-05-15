# Install and Run SPARTA for Tudat on TU Delft Servers

1. Get username and password to access the server
2. Create a new directory where to install the software. For instance, to create a repository called `sparta`:
	~~~unix
	mkdir ~/sparta/
	~~~
This will create a new directory in your main folder. 
3. Clone this repository to the folder you just created by running:
	~~~unix
	cd sparta
	git clone https://github.com/mfacchinelli/sparta
	~~~
4. Compile the SPARTA source code by typing the following commands:
	~~~unix
	cd ~/sparta/src/
	make -j 14 mpi
	~~~
Now SPARTA should be making the executable that will then be used to run the simulations. The compilation should be very quick thanks to the 14 cores (called with the `-j` flag). The executable will be created in the `~/sparta/src/` directory under the name `spa_mpi` (if you run the lines above with no modifications). 

You are now ready to run SPARTA applications. It is highly recommended to first read the manual and play around with the examples provided (see repository `examples`), to get some experience with the language used by the `in.XXX` files.  You can follow the remaining steps if you want to verify the correct installation of the software. 

5. Run the sphere example with the commands:
	~~~unix
	cd ~/sparta/examples/sphere
	mpirun -np 14 ../sparta/src/spa_mpi -in in.mro
	~~~
Note that the paths and/or file names will need to be adapted in case you compiled a different `MAKEFILE` or chose a different name for the SPARTA repository. If everything goes well, SPARTA will output a bunch of lines in the terminal window, and in the same folder you will find 5 new files, called something like `force.0`, `force.200`, etc. These are the values of the average pressure and shear force computed by SPARTA every 200 time steps, for each triangle making up the sphere we are analyzing. You can find more information on the `dump` commands in the manual (which is also installed by step 3, and can be found under `~/sparta/Manual.pdf`). 
6. Now open the file called `force.600` and compare the results with the values below. Note that to open the file, you can use *either* of these commands:
	~~~unix
	vi ~/sparta/examples/sphere/force.600
	nano ~/sparta/examples/sphere/force.600
	more ~/sparta/examples/sphere/force.600
	~~~
The first few lines should look something like this:
	~~~
	ITEM: TIMESTEP
	600
	ITEM: NUMBER OF SURFS
	1200
	ITEM: BOX BOUNDS oo oo oo
	-2 2
	-2 2
	-2 2
	ITEM: SURFS id f_1[1] f_1[2] f_1[3] f_1[4] f_1[5] f_1[6] 
	1 2.09519e-21 1.8073e-21 1.93534e-21 -1.55814e-37 -1.55814e-37 -2.33721e-37 
	15 0 0 0 0 0 0 
	29 4.1913e-22 3.92169e-22 -2.22258e-22 -5.09671e-38 -5.09671e-38 2.54835e-38 
	43 0 0 0 0 0 0 
	...
	~~~
The first 9 lines confirm what the user has input, i.e., that the `dump` files are created as a function of time step and that the geometry (the sphere) is make up of 1200 triangles (or surfaces) and that the simulation environment has open bounds that extend from -2 to 2 along each axis. Then the file shows each element of the pressure and shear forces vectors (so p_1, p_2, p_3, s_1, s_2 and s_3) for each surface element. The ID of the surface element(`SURFS id` in the file) is in the first column, and is in a scrambled order due to the usage of multiple cores (see the manual for more information). If the lines above match the output of your simulation, then it would appear that the installation went well! If not, try to figure out what went wrong yourself first, and otherwise open a new issue on GitHub.