# SPARTA for Tudat on TU Delft Servers

SPARTA is an open-source software, copyright of the Sandia Corporation, which analyses the aerodynamics of a vehicle in rarefied flow conditions. 

The code provided in this repository is directly copied from the developer website (http://sparta.sandia.gov) and is available under the GNU General Public License. 

The version available in this repository is updated to the latest version (**4 Apr 2018 version**). Note that when downloading SPARTA directly from the developer's website, more files are made available (including a Python interface). These are not provided in this repository, since they are not used for the Tudat interface.

## Installation and Running

1. Get username and password to access the server and log in
2. Create a new directory where to install the software. For instance, to create a repository called `sparta`:
	```unix
	mkdir ~/sparta/
	```
	This will create a new directory in your main folder. 
3. Clone this repository to the folder you just created by running:
	```unix
	git clone https://github.com/mfacchinelli/sparta.git ~/sparta/
	```
4. Compile the SPARTA source code by typing the following commands:
	```unix
	cd ~/sparta/src/
	make -j 14 mpi
	```
	Now SPARTA should be making the executable that will then be used to run the simulations. The compilation should be very quick thanks to the use of 14 cores (called with the `-j` flag). The executable will be created in the `~/sparta/src/` directory under the name `spa_mpi` (if you run the lines above with no modifications). 

You are now ready to run SPARTA applications. It is highly recommended to first read the manual and play around with the examples provided (see repository `~/sparta/examples/`), to get some experience with the language used by the `in.XXX` files.  You can follow the remaining steps if you want to verify the correct installation of the software. 

5. Run the sphere example with the commands:
	```unix
	cd ~/sparta/examples/sphere/
	mpirun -np 14 ~/sparta/src/spa_mpi -in in.sphere
	```
	Note that the paths and/or file names will need to be adapted in case you compiled a different `MAKEFILE` or chose a different name for the SPARTA repository. If everything goes well, SPARTA will output a bunch of lines in the terminal window, and in the same folder you will find 5 new files, called something like `force.0`, `force.200`, etc. These are the values of the average pressure and shear force computed by SPARTA every 200 time steps, for each triangle making up the sphere we are analyzing. You can find more information on the `dump` commands in the manual (which is also installed by step 3, and can be found under `~/sparta/Manual.pdf`). 
6. Now open the file called `force.600` and compare the results with the values below. Note that to open the file, you can use *either* of these commands:
	```unix
	vi ~/sparta/examples/sphere/force.600
	nano ~/sparta/examples/sphere/force.600
	more ~/sparta/examples/sphere/force.600
	```
	The first few lines should look something like this:
	```
	ITEM: TIMESTEP
	600
	ITEM: NUMBER OF SURFS
	1200
	ITEM: BOX BOUNDS oo oo oo
	-2 2
	-2 2
	-2 2
	ITEM: SURFS id f_1[1] f_1[2] f_1[3] f_1[4] f_1[5] f_1[6]
	1 0 0 0 0 0 0 
	15 0 0 0 0 0 0 
	29 0.00519895 0.00486452 -0.00275691 0 0 5.09671e-19 
	43 0 0 0 0 0 0 
	57 0.0178258 0.0121653 0.00183404 -5.2281e-18 -2.61405e-18 -6.53513e-19 
	...
	```
	The first 9 lines confirm what the user has input, i.e., that the `dump` files are created as a function of time step, that the geometry (the sphere) is make up of 1200 triangles (or surfaces) and that the simulation environment has open bounds that extend from -2 to 2 along each axis. Then the file shows each element of the pressure and shear forces vectors (so p<sub>1</sub>, p<sub>2</sub>, p<sub>3</sub>, s<sub>1</sub>, s<sub>2</sub> and s<sub>3</sub>) for each surface element. The ID of the surface element (`SURFS id` in the file) is in the first column, and is in a 'scrambled' order due to the usage of multiple cores (see the manual for more information). If the lines above match the output of your simulation, then it would appear that the installation went well! If not, and you are using a different machine than the TU Delft servers, then it may have to do with that. However, the order of magnitude should still be the same. Otherwise, try to figure out what went wrong yourself first, and otherwise open a new issue on GitHub.

## Documentation

You can find the manual in this repository (`Manual.pdf`), in the clone of your repository (`~/sparta/Manual.pdf`), or on the developer website (http://sparta.sandia.gov/doc/Manual.html). Read the manual carefully, especially if you need to create your own geometry, and your own `in.XXX` files. 

In the folder `~/sparta/data/` some example geometry files are provided (Space Shuttle, Orion capsule, sphere, etc.).

## Atmosphere Modeling

To run SPARTA you will need to know some very specific thermodynamic properties of the gases composing the atmosphere (or rather the airflow). SPARTA comes with some files (usually named something like `air.vss` and `air.species`) where these properties are listed for the most common gases in the Earth's atmosphere. 

However, atmospheres of other planets are very different, thus these values cannot always be used, or different gases are needed. Soon, a more comprehensive list of gases and their properties will be provided. 
