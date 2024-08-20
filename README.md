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
	


