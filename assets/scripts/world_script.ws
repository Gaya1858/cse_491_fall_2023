_="List of cell types"
floor=addCellType("floor","An empty space"," ")
wall=addCellType("wall","A solid wall","#",CELL_WALL)
one=addCellType("one","The number 1","1",CELL_WALL)
two=addCellType("water","A water tile","w",CELL_WATER)
three=addCellType("three","The number 3","3","Collectible")

_="Load the actual world"
loadWorld("assets/grids/lang_load_test.grid")

_="Place the player agent into the world"
player=addAgent("Player","PlayerAgent","%",1,1)
setAgentProperty(player,"DoAction","../assets/scripts/player_move.ws")
