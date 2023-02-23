# Bubbles!
By Zexxerd

### How to build
First, go here (https://ce-programming.github.io/toolchain/static/getting-started.html) and follow the instructions.

Next, use your terminal to clone this repository to your computer. After that, enter its directory, type `make gfx`, and then type `make` or `make debug`. The whole process will look something like this:
```
#Install CEdev to home directory
cd desired_directory
git clone "https://github.com/Zexxerd/Bubbles.git"
cd bubbles
make gfx
make
```

After the project compiles, you can find the program in the bin folder as `BUBBLES.8xp`.

### Basic controls
Left and right move the shooter left and right. 2nd shoots a bubble. Clear exits the program.

#### Debugging
The arrow keys change the highlighted bubble.

Alpha shows the highlighted bubble's neighbors.

Mode shows the cluster created from the highlighted bubble's same-color adjacents.

Del hides and unhides a bubble.

GraphVar (X,T,Theta,n) changes the highlighted bubble's color to the shooter's color.

Math and Apps change the color of the shooter.

Prgm shows the available colors in the grid.

Vars shows the bubbles that a projectile should be able to collide with. 
