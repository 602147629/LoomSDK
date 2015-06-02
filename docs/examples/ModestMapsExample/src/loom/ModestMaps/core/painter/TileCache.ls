package loom.modestmaps.core.painter
{
    import loom.modestmaps.core.Tile;
        
    /** the alreadySeen Dictionary here will contain up to grid.maxTilesToKeep Tiles */
    public class TileCache
    {
        // Tiles we've already seen and fully loaded, by key (.name)
        protected var alreadySeen:Dictionary.<String, Tile>;
        protected var tilePool:TilePool; // for handing tiles back!
        
        public function TileCache(tilePool:TilePool)
        {
            this.tilePool = tilePool;
            alreadySeen = new Dictionary.<String, Tile>();
        }
        
        public function get size():int
        {
            return alreadySeen.length;
        }
        
        public function putTile(tile:Tile):void
        {
            //trace("PUT", tile.name);
            if (containsKey(tile.name)) trace("Already contains", tile.name, getTile(tile.name));
            alreadySeen[tile.name] = tile;
        }
        
        public function getTile(key:String):Tile
        {
            //trace("GET", key);
            return alreadySeen[key];
        }
        
        public function containsKey(key:String):Boolean
        {
            return (alreadySeen[key] != null);
        }
        
        public function returnKey(key:String):Tile
        {
            //trace("DEL", key);
            var tile = alreadySeen[key];
            if (!tile) return null;
            tilePool.returnTile(tile);
            alreadySeen.deleteKey(key);
            return tile;
        }
        
        public function retainKeys(keys:Vector.<String>):void
        {
            for (var key:String in alreadySeen) {
                if (keys.indexOf(key) < 0) {
                    tilePool.returnTile(alreadySeen[key]);
                    alreadySeen.deleteKey(key);
                }
            }       
        }
        
        public function clear():void
        {
            for (var key:String in alreadySeen) {
                tilePool.returnTile(alreadySeen[key]);
            }
            alreadySeen.clear();        
        }
    }
}