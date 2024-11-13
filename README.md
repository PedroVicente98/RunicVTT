Let’s break this down into two main parts:

1. **Grid Rendering**:
   - For **Square Grids** and **Hex Grids**, we need to figure out how to calculate the positions for rendering.
   - We also need the ability to **modify** the grid’s scale and position and **adjust the grid size** based on the image size plus a 10% margin.

2. **Snap to Grid**:
   - How to **snap entities** (markers) to either square or hex grid coordinates.

### 1. Grid Rendering Logic (Square and Hex)

#### **Square Grid Rendering**

- **Scale**: The scale of the grid is the size of each square. The grid will have a fixed number of squares to fill the board, so the scale can increase or decrease, affecting how many squares fit within the grid.
- **Offset**: The grid's position can be moved, so we need to account for the offset when rendering the gridlines.
- **Rows/Columns Calculation**:
    - Number of **columns** = `(mapWidth / gridScale) + 10% padding`
    - Number of **rows** = `(mapHeight / gridScale) + 10% padding`
- **Positioning**:
    - Each square has an origin at its top-left corner. The x/y positions for each vertical and horizontal line are spaced by `gridScale` in both directions, offset by `gridOffset`.

##### Math for the Square Grid

1. **Grid Scale (gridSize)**: This defines the width/height of each square in world space.

2. **Grid Offset (gridOffset)**: 
   - This is applied to all grid positions (both lines and squares), allowing the grid to move independently of the board.

3. **Rendering Grid Lines**:
   - Vertical lines are rendered for every x-coordinate: 
     \[
     x = \text{offset.x} + n \times \text{gridScale}, \quad \text{where } n \in [0, \text{number of columns}]
     \]
   - Horizontal lines are rendered for every y-coordinate: 
     \[
     y = \text{offset.y} + m \times \text{gridScale}, \quad \text{where } m \in [0, \text{number of rows}]
     \]
  
The 10% buffer is added by adjusting the total number of rows/columns:
\[
\text{numRows} = \left\lceil \frac{\text{mapHeight}}{\text{gridScale}} \times 1.1 \right\rceil
\]
\[
\text{numCols} = \left\lceil \frac{\text{mapWidth}}{\text{gridScale}} \times 1.1 \right\rceil
\]

#### **Hex Grid Rendering**

- **Hexagons** are a bit more complicated since their shapes don’t fit perfectly into a square grid. Hexagons are typically arranged in a "staggered" pattern where each row is offset horizontally by half the hex width.
  
##### Math for Hexagons

1. **Hexagon Dimensions**:
    - Hexagons are defined by **hexWidth** and **hexHeight** (derived from `gridScale`).
    - For hex grids:
      \[
      \text{hexWidth} = \text{gridScale}
      \]
      \[
      \text{hexHeight} = \frac{\sqrt{3}}{2} \times \text{gridScale}
      \]

2. **Offset Staggered Rows**:
   - Even rows are rendered normally, but **odd rows** are staggered by half the width of a hexagon.
  
3. **Rendering Hex Lines**:
    - For every y-coordinate (vertical position): 
      \[
      y = \text{offset.y} + m \times 3/4 \times \text{hexHeight}
      \]
    - For every x-coordinate (horizontal position):
      - Even rows:
        \[
        x = \text{offset.x} + n \times \text{hexWidth}
        \]
      - Odd rows (staggered by half a hexWidth):
        \[
        x = \text{offset.x} + (n \times \text{hexWidth}) + \frac{1}{2} \times \text{hexWidth}
        \]
    - This staggered layout ensures a proper hexagonal grid structure.

4. **Number of Rows/Columns**:
    - Like the square grid, the number of rows and columns is calculated based on map dimensions and grid size.

### 2. Snap to Grid Logic (Square and Hex)

#### **Square Grid Snap**

1. **Marker Position**:
    - The marker’s position should be snapped to the nearest square grid cell.
    - **Formula**:
      \[
      \text{snappedPos.x} = \left\lfloor \frac{markerPos.x - gridOffset.x}{gridScale} \right\rfloor \times gridScale + gridOffset.x
      \]
      \[
      \text{snappedPos.y} = \left\lfloor \frac{markerPos.y - gridOffset.y}{gridScale} \right\rfloor \times gridScale + gridOffset.y
      \]
    - This ensures that the marker aligns with the closest square on the grid.

#### **Hex Grid Snap**

1. **Marker Position in Hex**:
    - For hexagonal grids, the snapping process is more complicated because of the staggered rows.
    - **Axial Coordinates** are typically used to map hexagons in a grid. Axial coordinates (q, r) represent the hex grid as two axes, one for rows and one for diagonal columns.
  
2. **Convert World Position to Axial Coordinates**:
    - Convert the marker’s world position to its corresponding hexagonal cell:
      \[
      q = \frac{\sqrt{3}}{3} \times \frac{markerPos.x - gridOffset.x}{gridScale}
      \]
      \[
      r = \frac{2}{3} \times \frac{markerPos.y - gridOffset.y}{gridScale}
      \]
  
3. **Snapping to Nearest Hex**:
    - Round q and r to the nearest integer and then convert back to world coordinates to place the marker in the nearest hex cell.

### Putting it All Together (Logic Flow)

1. **Render the Grid**:
    - For both square and hex, calculate the number of rows and columns based on the grid scale and map size.
    - Draw the grid lines based on the current **gridScale** and **gridOffset**.

2. **Grid Manipulation**:
    - If the **grid tool** is selected:
      - **Drag**: Modify `gridOffset` to move the grid.
      - **Scroll**: Adjust `gridScale` to zoom in/out, which will change the number of rows/columns accordingly.

3. **Snap to Grid**:
    - When placing a marker, convert the marker’s world position to the nearest grid cell using the snapping logic for square or hex.
    - Use the formulas above to compute the nearest grid location and place the marker accordingly.

---

This should cover the fundamental math and logic for rendering grids (square/hex) and snapping entities to them. You can now implement this based on your preferred flow!


# RunicVTT
Open Source Virtual Tabletop using OpengGL, ImGUI, ImGUI Markdown and P2P Network

# ApplicationHandler
## Attributes
- GLFWWindow* window_handler
- GameTable* active_window

## Methods - Commom GUI
- renderMainMenuBar
- setup

## Methods - GameTable GUI
- renderCreateGameTablePopUp
- renderLoadGameTablePopUp
- renderSaveGameTablePopUp
- renderActiveGameTable

## Methods - GameTable
- createGameTable
- setActiveGameTable
- loadGameTable
- saveGameTable
- closeGameTable

## Methods - Notes
- createNote
- loadNote
- saveNote
- closeNote

- ## Methods - Notes GUI
- renderCreateNotePopUp
- renderLoadNotePopUp
- renderSaveNotePopUp
- renderNotesDirectory
- renderNoteEditor

# GameTable
## Methods - Commom GUI
- renderMenu
- serializeData

## Methods - Board
- createBoard
- saveBoard
- loadBoard
- loadMapImage
- setActiveBoard

## Methods - Connection
- openConnection
- closeConnection
- saveConnection

## Methods - Notes
- createNote
- loadNote
- saveNote
- openNotesDirectory

# Board
## Methods - Commom GUI
- renderToolbar
- renderMap
- handleInput
- renderFogOfWar
- toggleFogOfWar
- cutFogOfWar
- createFogOfWar
- deleteFogOfWar

## Methods - Marker
- createMarker
- handleMarkers
- deleteMarker


# Marker
## Methods - Commom GUI
- renderEditWindow
- renderMarker
- 

----------------------------------------------------------------------------------------------------------------------------------------------------
# Classes Methods
- Board
	- handleInput()	- always before draw, to make the actions
	- draw - draw updated actions
- Marker
	- handleInput()	
	- draw
- GameTable
	- handleInput()	
	- draw

# Canvas
	# Asset Directory
	# Board
	# Chat
	# NetworkLink

# Network
	# Nodes
	# Senders
	# Receivers
	# DataSync

# Notes
	# Notes Directory
	# Notes Markdown
	# Notes Linking - Add to inventory funtion with this
	# Notes Categorization
	# Notes Integration
		# ChatLink
		# NetworkLink

# Windows - ImGUI Dockables
	#Board
	#Chat
	#Notes - with tabs
	#Notes Directory
	#Asset Directory
	


