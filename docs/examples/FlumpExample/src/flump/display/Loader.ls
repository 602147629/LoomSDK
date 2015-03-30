//
// Flump - Copyright 2013 Flump Authors

package flump.display {

import deng.fzip.FZip;
import deng.fzip.FZipErrorEvent;
import deng.fzip.FZipEvent;
import deng.fzip.FZipFile;
import loom.LoomBinaryAsset;
import loom.platform.Timer;
import loom2d.Loom2D;

import loom2d.events.Event;
//import loom2d.events.IOErrorEvent;
//import loom2d.events.ProgressEvent;
import loom2d.math.Point;
import loom2d.math.Rectangle;
//import loom2d.net.URLRequest;

import flump.executor.Executor;
import flump.executor.Future;
import flump.executor.FutureTask;
//import flump.executor.load.ImageLoader;
//import flump.executor.load.LoadedImage;
import flump.mold.AtlasMold;
import flump.mold.AtlasTextureMold;
import flump.mold.LibraryMold;
import flump.mold.MovieMold;
import flump.mold.TextureGroupMold;

import loom2d.textures.Texture;

class Loader {
    public function Loader (toLoad :Object, libLoader :LibraryLoader) {
        _scaleFactor = (libLoader.scaleFactor > 0 ? libLoader.scaleFactor :
            Loom2D.contentScaleFactor);
        _libLoader = libLoader;
        _toLoad = toLoad;
        
        _zip.addEventListener(Event.COMPLETE, onZipLoadingComplete);
        _zip.addEventListener(FZipEvent.FILE_LOADED, onFileLoaded);
    }

    public function load (future :FutureTask) :void {
        _future = future;
        
        _zip.removeEventListeners(FZipErrorEvent.PARSE_ERROR);
        _zip.addEventListener(FZipErrorEvent.PARSE_ERROR, _future.fail);
        
        if (_toLoad is String) loadLiveFile(String(_toLoad));
        else if (_toLoad is ByteArray) {
            reset();
            _zip.loadBytes(ByteArray(_toLoad));
        }
        else Debug.assert(false, "Unsupported Flump Loader type");
    }
    
    protected function loadLiveFile(path:String) {
        persistent = true;
        var asset:LoomBinaryAsset = LoomBinaryAsset.create(path);
        asset.updateDelegate += onLiveLoaded;
        asset.load();
    }
    
    protected function onLiveLoaded(path:String, contents:ByteArray):void {
        trace("Loaded "+path+" ("+contents.length+" bytes)");
        reset();
        _zip.loadBytes(contents);
    }
    

    protected function onFileLoaded (e :FZipEvent) :void {
        const loaded :FZipFile = _zip.removeFileAt(_zip.getFileCount() - 1);
        const name :String = loaded.filename;
        if (name == LibraryLoader.LIBRARY_LOCATION) {
            const jsonString :String = loaded.content.readUTFBytes(loaded.content.length);
            _lib = LibraryMold.fromJSON(JSON.parse(jsonString));
            _libLoader.libraryMoldLoaded(_lib);
        } else if (name.indexOf(PNG, name.length - PNG.length) != -1) {
            _atlasBytes[name] = loaded.content;
        } else if (name.indexOf(ATF, name.length - ATF.length) != -1) {
            _atlasBytes[name] = loaded.content;
            _libLoader.atfAtlasLoaded({name: name, bytes: loaded.content});
        } else if (name == LibraryLoader.VERSION_LOCATION) {
            const zipVersion :String = loaded.content.readUTFBytes(loaded.content.length);
            Debug.assert(zipVersion == LibraryLoader.VERSION, "Zip is version " + zipVersion + " but the code needs " + LibraryLoader.VERSION);
            _versionChecked = true;
        } else if (name == LibraryLoader.MD5_LOCATION ) { // Nothing to verify
        } else {
            _libLoader.fileLoaded({name: name, bytes: loaded.content});
        }
    }

    protected function onZipLoadingComplete (..._) :void {
        if (!persistent) _zip = null;
        Debug.assert(_lib != null, LibraryLoader.LIBRARY_LOCATION + " missing from zip");
        Debug.assert(_versionChecked, LibraryLoader.VERSION_LOCATION + " missing from zip");
        //const loader :ImageLoader = _lib.textureFormat == "atf" ? null : new ImageLoader();
        
        // Determine the scale factor we want to use
        var textureGroup :TextureGroupMold = _lib.bestTextureGroupForScaleFactor(_scaleFactor);
        if (textureGroup != null) {
            for each (var atlas :AtlasMold in textureGroup.atlases) {
                loadAtlas(atlas);
            }
        }
        // free up extra atlas bytes immediately
        for (var leftover :String in _atlasBytes) {
            if (_atlasBytes.hasOwnProperty(leftover)) {
                ByteArray(_atlasBytes[leftover]).clear();
                _atlasBytes.deleteKey(leftover);
            }
        }
        onPngLoadingComplete();
    }

    protected function loadAtlas ( atlas :AtlasMold) :void {
        const bytes :ByteArray = _atlasBytes[atlas.file];
        _atlasBytes.deleteKey(atlas.file);
        Debug.assert(bytes != null, "Expected an atlas '" + atlas.file + "', but it wasn't in the zip");
        
        bytes.position = 0; // reset the read head
        var scale :Number = atlas.scaleFactor;
        
        Debug.assert(_lib.textureFormat != "atf", "ATF image format not supported");
        
        var tex:Texture = Texture.fromBytes(bytes);
        _libLoader.pngAtlasLoaded( { atlas: atlas, image: tex } ); 
        baseTextureLoaded(tex, atlas);
        
        bytes.clear();
    }

    protected function baseTextureLoaded (baseTexture :Texture, atlas :AtlasMold) :void {
        _baseTextures.push(baseTexture);

        _libLoader.creatorFactory.consumingAtlasMold(atlas);
        var scale :Number = atlas.scaleFactor;
        for each (var atlasTexture :AtlasTextureMold in atlas.textures) {
            var bounds :Rectangle = atlasTexture.bounds;
            var offset :Point = atlasTexture.origin;
            
            // Starling expects subtexture bounds to be unscaled
            if (scale != 1) {
                bounds = bounds.clone();
                bounds.x /= scale;
                bounds.y /= scale;
                bounds.width /= scale;
                bounds.height /= scale;
                
                offset = offset.clone();
                offset.x /= scale;
                offset.y /= scale;
            }
            
            _creators[atlasTexture.symbol] = _libLoader.creatorFactory.createImageCreator(
                atlasTexture,
                Texture.fromTexture(baseTexture, bounds),
                offset,
                atlasTexture.symbol);
        }
    }

    protected function onPngLoadingComplete () :void {
        for each (var movie :MovieMold in _lib.movies) {
            movie.fillLabels();
            _creators[movie.id] = _libLoader.creatorFactory.createMovieCreator(
                movie, _lib.frameRate);
        }
        _future.succeed(new LibraryImpl(_baseTextures, _creators, _lib.isNamespaced));
    }

    protected function onPngLoadingFailed (e :Object) :void {
        if (_future.isComplete) return;
        _future.fail(e);
    }
    
    protected function reset() {
        _lib = null;
        _future.reset();
        
        _versionChecked = false;
        
        for each (var tex:Texture in _baseTextures) {
            tex.dispose();
        }
        _baseTextures.clear();
        
        _creators.clear();
        _atlasBytes.clear();
    }

    protected var persistent:Boolean = false;
    protected var _toLoad :Object;
    protected var _scaleFactor :Number;
    protected var _libLoader :LibraryLoader;
    protected var _future :FutureTask;
    protected var _versionChecked :Boolean;

    protected var _zip :FZip = new FZip();
    protected var _lib :LibraryMold;

    protected const _baseTextures :Vector.<Texture> = new <Texture>[];
    protected const _creators = new Dictionary.<String, SymbolCreator>();
    protected const _atlasBytes = new Dictionary.<String, ByteArray>();

    protected static const PNG :String = ".png";
    protected static const ATF :String = ".atf";
}
}
