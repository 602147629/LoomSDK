/*
 * vim:et sts=4 sw=4 cindent:
 * $Id$
 */

package loom.modestmaps.core
{
    import loom2d.display.DisplayObject;
    import loom2d.display.Sprite;
    import loom2d.display.Image;
    import loom2d.textures.Texture;
    
    
    public class Tile extends Sprite
    {       
        public static var count:int = 0;
        
        // not a coordinate, because it's very important these are ints
        public var zoom:int;
        public var row:int;
        public var column:int;

        protected var requestedTextures:Vector.<Texture> = [];
        protected var assignedTextures:Vector.<Texture> = [];
                

        public function Tile(column:int, row:int, zoom:int)
        {
            init(column, row, zoom);
                        
            count++;
        } 
        
        /** override this in a subclass and call grid.setTileCreator if you want to draw on your tiles */
        public function init(column:int, row:int, zoom:int):void
        {
            this.zoom = zoom;
            this.row = row;
            this.column = column;           
            hide();
        }        

        /** once TileGrid is done with a tile, it will call destroy and possibly reuse it later */
        public function destroy():void
        {
            //clean up all textures
            var i:int;
            for(i=0;i<requestedTextures.length;i++)
            {
                requestedTextures[i].cancelHTTPRequest();
            }
            for(i=0;i<assignedTextures.length;i++)
            {
//TODO_24: Need a texture ref counter as there are cases when the same texture can be used for multiple tiles...
                assignedTextures[i].dispose();
            }
            assignedTextures.clear();
            requestedTextures.clear();

            //dispose all child Images
            while (numChildren > 0) {
                var child:DisplayObject = removeChildAt(0);
                
                //dispose the image data
                if(child is Image)
                {
                    child.dispose();
                }
            }
        }

        public function requestTexture(texture:Texture):void
        {
            //store texture in a vector so we can track all of them
            requestedTextures.pushSingle(texture);
        }

        public function assignTexture(texture:Texture):Image
        {
            //create an image for the newly loaded texture and add it to the tile
//TODO_24: Need a texture ref counter as there are cases when the same texture can be used for multiple tiles...
            var img:Image = new Image(texture);                    
            addChild(img);

            //make sure it's not in our requested list still
            requestedTextures.remove(texture);

            //store texture in a vector so we can track all of them
            assignedTextures.pushSingle(texture);
            return img;
        }

        public function removeRequestedTexture(texture:Texture):void
        {
            requestedTextures.remove(texture);
        }

        public function isUsingTexture(texture:Texture):Boolean
        {
            return (assignedTextures.contains(texture) || requestedTextures.contains(texture)) ? true : false;
        }
        
        public function isShowing():Boolean
        {
            return this.alpha == 1.0;
        }
        
        public function showNow():void
        {
            this.alpha = 1.0;
        }
        
        public function show():void 
        {
            this.alpha = 1.0;
            // if you want to do something when the tile is ready then override this method
        }
        
        public function hide():void
        {
            this.alpha = 0.0;
        }
        
        public function paintError(w:Number=256, h:Number=256):void
        {
            //TODO_TEC: Show an error visually for this tile... display an X texture or something?
            trace("ERROR setting a texture for this tile!");
        }

        public function toString():String
        {
            return "(" + column + ", " + row + ", " + zoom + ")";
        }
    }
}
