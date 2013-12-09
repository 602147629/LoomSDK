package
{

    import loom.Application;

    import system.platform.Platform;
    import loom2d.display.StageScaleMode;    
    import loom2d.display.Image;
    import loom2d.display.QuadBatch;
    import loom2d.textures.Texture;

    import loom2d.text.BitmapFont;

    import loom2d.events.Touch;
    import loom2d.events.TouchEvent;
    import loom2d.events.TouchPhase;  

    import loom.gameframework.LoomGroup;
    import loom.gameframework.TimeManager;
    import loom.box2d.*;

    public class PhysicsGameObject extends Object
    {
        public var sprite:Image;
    }

    public class Box2DExample extends Application
    {
        var pixelsPerMeter:Number = 30;

        var simulationEnabled:Boolean = false;
        var tickRate:Number = 1/60;
        var velocityIterations:int = 6;
        var positionIterations:int = 2;

        var world:b2World;

        public function onFrame():void
        {
            if (!simulationEnabled)
                return;

            world.step(tickRate, velocityIterations, positionIterations);
            
            var b:b2Body = world.getBodyList();
            while (b)
            {
                var pgo = b.getUserData() as PhysicsGameObject;
                if (!pgo)
                    continue;

                // make objects update their sprites
                pgo.sprite.x = b.getPosition().x * pixelsPerMeter;
                pgo.sprite.y = b.getPosition().y * pixelsPerMeter;
                pgo.sprite.rotation = b.getAngle();

                // clean up bodies that fell out of the visible area
                // actually, removing the sprite from the stage is a hit on performance
                // and the world does a great job GCing bodies
                /*
                if (b.getPosition().y > (stage.stageHeight + Math.max(pgo.sprite.width, pgo.sprite.height))/pixelsPerMeter)
                {
                    stage.removeChild(pgo.sprite);
                    world.destroyBody(b);
                }
                */

                b = b.getNext();
            }
        }

        public function createBox(world:b2World, type:int, position:b2Vec2, rotation:Number, dimensions:b2Vec2, imagePath:String, density:Number, friction:Number):b2Body
        {
            return createBody(world, type, position, rotation, dimensions, dimensions.x/2, imagePath, density, friction, "box");
        }

        public function createCircle(world:b2World, type:int, position:b2Vec2, rotation:Number, radius:Number, imagePath:String, density:Number, friction:Number):b2Body
        {
            return createBody(world, type, position, rotation, new b2Vec2(radius*2, radius*2), radius, imagePath, density, friction, "circle");
        }

        public function createBody(world:b2World, type:int, position:b2Vec2, rotation:Number, dimensions:b2Vec2, radius:Number, imagePath:String, density:Number, friction:Number, shapeType:String="box"):b2Body
        {
            // create a body
            var bodyDef:b2BodyDef = new b2BodyDef();
            bodyDef.type = type; // 2-dynamic, 1-kinematic, 0-static
            bodyDef.position = position;
            var body:b2Body = world.createBody(bodyDef);

            // create a game object for the body
            var goBody:PhysicsGameObject = new PhysicsGameObject();
            goBody.sprite = new Image(Texture.fromAsset(imagePath));
            goBody.sprite.center();
            goBody.sprite.width = dimensions.x * pixelsPerMeter;
            goBody.sprite.height = dimensions.y * pixelsPerMeter;
            goBody.sprite.x = body.getPosition().x * pixelsPerMeter;
            goBody.sprite.y = body.getPosition().y * pixelsPerMeter;
            goBody.sprite.rotation = rotation;
            stage.addChild(goBody.sprite);

            // attach the body game object to the body body
            body.setUserData(goBody as Object);

            // create a fixture for the body body with the body shape
            var fixtureBody:b2FixtureDef = new b2FixtureDef();
            fixtureBody.density = density;
            fixtureBody.friction = friction;

            // create a shape for the body - one of a few different shapes
            switch (shapeType)
            {
                case "circle":
                    var staticBodyCirc:b2CircleShape = new b2CircleShape();
                    staticBodyCirc.radius = radius;
                    fixtureBody.shape = staticBodyCirc;
                    break;
                default: // box
                    var staticBodyRect:b2PolygonShape = new b2PolygonShape();
                    staticBodyRect.setAsBox(dimensions.x/2, dimensions.y/2);
                    fixtureBody.shape = staticBodyRect;
                    break;
            }

            body.createFixture(fixtureBody);

            return body;
        }

        public function createRandomShape(px:Number, py:Number)
        {
            // find a random shape
            var body:b2Body;
            if (Math.random()<0.5)
                body = createBox(world, 2, new b2Vec2(px, py), 0, new b2Vec2(Math.random()*2+0.25, Math.random()*2+0.25), "assets/square.png", 1, 0.3);
            else
                body = createCircle(world, 2, new b2Vec2(px, py), 0, Math.random()+0.125, "assets/circle.png", 1, 0.3);            

            // apply a random force
            body.applyForceToCenter(new b2Vec2(Math.random()*2000-1000, Math.random()*2000-1000), true);
            body.applyTorque(Math.random()*2000-1000, true);
        }

        override public function run():void
        {

            stage.scaleMode = StageScaleMode.NONE;

            var bg = new Image(Texture.fromAsset("assets/bg.png"));
            bg.x = 0;
            bg.y = 0;
            bg.width = stage.nativeStageWidth;
            bg.height = stage.nativeStageHeight;
            stage.addChild(bg);

            // *******

            // Create the world and set up gravity.
            // Earth (-9.78), Moon (-1.622), Mars (-3.711), Venus (-8.87)
            // Using a gravity vector pointing downwards on the screen.
            var gravity:b2Vec2 = new b2Vec2(0, 9.78);
            world = new b2World(gravity);

            // stage in meters
            var mWidth:Number = stage.stageWidth/pixelsPerMeter;
            var mHeight:Number = stage.stageHeight/pixelsPerMeter;

            // create a floor (center, bottom)
            createBox(world, 0, new b2Vec2(mWidth*0.5, mHeight*0.9), 0, new b2Vec2(mWidth*0.75, mHeight*0.1), "assets/rect.png", 1, 0.3);

            // create falling shapes (full width, top half)
            for (var i:int = 0; i<50; i++)
                createRandomShape(Math.random()*mWidth, Math.random()*mHeight*0.5);

            // listen for touch and generate a new shape on touch
            stage.addEventListener( TouchEvent.TOUCH, function(e:TouchEvent)
            {
                var touch = e.getTouch(stage, TouchPhase.BEGAN);
                if (touch && simulationEnabled)
                for (var i:int = 0; i<10; i++)
                    createRandomShape(touch.globalX/pixelsPerMeter, touch.globalY/pixelsPerMeter);
            });            

            // set up tick rate and iterations
            var timeManager = (LoomGroup.rootGroup.getManager(TimeManager) as TimeManager);
            tickRate = timeManager.TICK_RATE;

            // enable the simulation so that onFrame handles physics stepping
            simulationEnabled = true;
        }

    }
}