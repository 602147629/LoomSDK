/*
===========================================================================
Loom SDK
Copyright 2011, 2012, 2013 
The Game Engine Company, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 
===========================================================================
*/

package tests {

import unittest.Assert;

[Native(managed)]
native class MyManagedNativeClass {
    
    native public var intField:int;
    native public var floatField:float;
    native public var doubleField:float;
    native public var stringField:String;

    native public function get intProperty():int;
    native public function set intProperty(value:int);
    
    native public var child:MyManagedNativeClass;
    
    public native function getDescString(value:int):String;
    public native function getDescStringBool(value:Boolean):String;
    public native function getChild():MyManagedNativeClass;
    
    public static native function addInstance(instance:MyManagedNativeClass);
    public static native function getNumInstances():int;
    public static native function getInstance(idx:int):MyManagedNativeClass;
    public static native function deleteRandomInstance();
    public static native function createdNativeInstance():MyManagedNativeClass;
    public static native function deleteNativeInstance(instance:MyManagedNativeClass);
    
    public var scriptString = "Hello, ";

    public function testDeleteB()
    {
        // delete this
        deleteNative();    
    }

    public function testDeleteA()
    {
        Assert.isFalse(nativeDeleted());
        testDeleteB();
        Assert.isTrue(this.nativeDeleted());
    }   
}

[Native(managed)]
final native class MyChildManagedNativeClass extends MyManagedNativeClass {

    public native function MyChildManagedNativeClass(stringArg:String = "default string");
    
    public native function getDescStringChildOverride(value:String):String;
    
    public static native function createMyChildManagedNativeClassNativeSide():MyChildManagedNativeClass;
    public static native function createMyChildManagedNativeClassAsMyManagedNativeClass():MyManagedNativeClass;
    
    public var scriptStringChild = "World!";

    public function get intProperty():int
    {
        return super.intProperty + 1;
    }

    public function set intProperty(value:int)
    {
        super.intProperty = value + 2;
    }
}

class TestManagedNativeClass
{
    function retrieveChild(owner:MyManagedNativeClass) {
        
        // get the child, and make sure the script value is still valid magic! 
        var child = owner.getChild();
        Assert.equal(child.scriptString, "Hello, ");
        Assert.equal((child as MyChildManagedNativeClass).scriptStringChild, "World!");
    }

    [Test]
    function testSharedInstance()
    {
        trace("A");
        var instance = new MyChildManagedNativeClass();
        Assert.equal(instance.scriptString, "Hello, ", "Failed to initalize simple class instance member.");

        trace("B");
        var child = new MyChildManagedNativeClass();
        Assert.equal(child.scriptString, "Hello, ", "Failed to initalize subclass instance members.");
        Assert.equal((child as MyChildManagedNativeClass).scriptStringChild, "World!", "Failed to initalize downcasted instance members.");

        instance.child = child;
        child = null;
        trace("C");
        GC.fullCollect();
        trace("D");
        child = instance.getChild() as MyChildManagedNativeClass;

        Assert.equal(child.scriptString, "Hello, ");
        Assert.equal((child as MyChildManagedNativeClass).scriptStringChild, "World!");
        trace("E");

        instance.deleteNative();
        child.deleteNative();
    }

    [Test]
    function testDowncast()
    {
        // We should be in a clean state.
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 0);
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);

        trace("A");
        var testdowncast = MyChildManagedNativeClass.createMyChildManagedNativeClassAsMyManagedNativeClass();
        Assert.equal(testdowncast.stringField, "created by createMyChildManagedNativeClassAsMyManagedNativeClass");
        Assert.equal(testdowncast.getType().getName(), "MyManagedNativeClass");
        Assert.equal(testdowncast.scriptString, "Hello, ");
        
        // set the script var, when we downcast below, the object initializer should
        // only be called for up to and not including the current type (if it was called it would 
        // reset this to "Hello!!!"
        trace("B");
        testdowncast.scriptString = "Happy New Year!!!";
        
        // native side has a MyChildManagedNativeClass instance, but we don't know about it 
        // as we have't downcasted yet
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);        
        
        trace("C");
        var downcast = testdowncast as MyChildManagedNativeClass;

        trace("D");

        // once we downcast, the managed system is updated with new RTTI
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 1);        
        
        Assert.equal(downcast, testdowncast);
        Assert.equal(downcast.stringField, "created by createMyChildManagedNativeClassAsMyManagedNativeClass");
        Assert.equal(downcast.getType().getName(), "MyChildManagedNativeClass");
        Assert.equal(downcast.scriptString, "Happy New Year!!!");
        Assert.equal(downcast.scriptStringChild, "World!");

        // now that we have downcast, testdowncase will automatically be promoted to better RTTI
        Assert.equal(testdowncast.stringField, "created by createMyChildManagedNativeClassAsMyManagedNativeClass");
        Assert.equal(testdowncast.getType().getName(), "MyChildManagedNativeClass");
        Assert.equal(testdowncast.scriptString, "Happy New Year!!!");
        
        // We can delete on the original reference
        testdowncast.deleteNative();
        
        // and the backend takes care of all the messy stuff
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 0);
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);
    }

    [Test]
    function test()
    {        
        var instance = new MyManagedNativeClass();

        Assert.equal(instance.intProperty, 101);
        Assert.isFalse(instance.nativeDeleted());        
        Assert.equal(instance.getDescString(100), "100 0 0.00 0.00");                
        Assert.equal(instance.getDescStringBool(true), "true 0 0.00 0.00");
        
        var child = new MyChildManagedNativeClass();

        //TODO: LOOM-1427
        //https://theengineco.atlassian.net/browse/LOOM-1427    
        //assert(child.intProperty == 102);
        
        Assert.equal(child.getDescStringChildOverride("hello"), "hello 1 1.00 1.00 default string");
        
        child.intField = 3000;
        child.doubleField = 10000;
        child.stringField = "goodbye";
        
        Assert.equal(child.getDescStringBool(false), "false 3000 1.00 10000.00 goodbye");
        
        // note this is a native field, GC isn't holding child
        instance.child = child;
        child = null;
        
        // call GC, though as MyChildManagedNativeClass has a managed class in it's inheritance graph
        // system will still hold onto it
        GC.fullCollect();
        
        retrieveChild(instance);
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 1);
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 1);

        instance.child.deleteNative();
        instance.deleteNative();
        
        Assert.isTrue(instance.nativeDeleted());
        
        
        // This will raise Access deleted managed native at: src/test/TestManagedNativeClass.ls 94
        // Console.print(instance.intField);
        // This will raise Access deleted managed native at: src/test/TestManagedNativeClass.ls 96
        // Console.print(instance.getDescString);
        
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 0);
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);
        
        var instances:Vector.<MyManagedNativeClass>  = new Vector.<MyManagedNativeClass>;
        var i:int;
        for (i = 0; i < 1000; i++) 
            instances.push(new MyManagedNativeClass);
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 1000);
        
        while(instances.length)
            instances.shift().deleteNative();
            
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 0);

        // create 1k managed natives with a mix of script and native instantiated        
        var iv:Vector.<MyManagedNativeClass> = new Vector.<MyManagedNativeClass>();
        for (i = 0; i < 1024; i++) {
            instance = Math.random() > 0.5 ? new MyManagedNativeClass() : MyManagedNativeClass.createdNativeInstance();
            MyManagedNativeClass.addInstance(instance);
            iv.pushSingle(instance);
        }
        
        for (i = 0; i < MyManagedNativeClass.getNumInstances(); i++) {
            
            instance = MyManagedNativeClass.getInstance(i);
            Assert.equal(iv[i].getNativeDebugString(), instance.getNativeDebugString(), "mismatch between script help and native returned managed");
            
        }
        
        var testi = new MyManagedNativeClass();
        testi.stringField = "This is a test";
        Assert.equal(testi.stringField, "This is a test");
        testi.deleteNative();
                
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 1024, "mismatch on MyManagedNativeClass count");
        
        // go through and delete random instances from C++ side (note some of these instances were created in script and some in C++
        for (i = 0; i < 512; i++) {
            MyManagedNativeClass.deleteRandomInstance();
        }
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 512, "mismatch on MyManagedNativeClass count post delete");
        
        // delete the rest
        for (i = 0; i < 512; i++) {
            MyManagedNativeClass.deleteRandomInstance();
        }
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 0, "lingering MyManagedNativeClass count post delete all");        
        
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);
        var nativeSide = MyChildManagedNativeClass.createMyChildManagedNativeClassNativeSide();
        Assert.equal(nativeSide.stringField, "created native side");        
        
        // REPRO CASE FOR LOOM
        Assert.equal(nativeSide.scriptString, "Hello, ");
        
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 1);
        
        nativeSide.deleteNative();
                
        var testdowncast = MyChildManagedNativeClass.createMyChildManagedNativeClassAsMyManagedNativeClass();
        Assert.equal(testdowncast.stringField, "created by createMyChildManagedNativeClassAsMyManagedNativeClass");
        Assert.equal(testdowncast.getType().getName(), "MyManagedNativeClass");
        Assert.equal(testdowncast.scriptString, "Hello, ");
        
        // set the script var, when we downcast below, the object initializer should
        // only be called for up to and not including the current type (if it was called it would 
        // reset this to "Hello!!!"
        testdowncast.scriptString = "Happy New Year!!!";
        
        // native side has a MyChildManagedNativeClass instance, but we don't know about it 
        // as we have't downcasted yet
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);        
        
        var downcast = testdowncast as MyChildManagedNativeClass;

        // once we downcast, the managed system is updated with new RTTI
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 1);        
        
        Assert.equal(downcast, testdowncast);
        Assert.equal(downcast.stringField, "created by createMyChildManagedNativeClassAsMyManagedNativeClass");
        Assert.equal(downcast.getType().getName(), "MyChildManagedNativeClass");
        Assert.equal(downcast.scriptString, "Happy New Year!!!");
        Assert.equal(downcast.scriptStringChild, "World!");

        // now that we have downcast, testdowncase will automatically be promoted to better RTTI
        Assert.equal(testdowncast.stringField, "created by createMyChildManagedNativeClassAsMyManagedNativeClass");
        Assert.equal(testdowncast.getType().getName(), "MyChildManagedNativeClass");
        Assert.equal(testdowncast.scriptString, "Happy New Year!!!");
        
        // We can delete on the original reference
        testdowncast.deleteNative();
        
        // and the backend takes care of all the messy stuff
        Assert.equal(Metrics.getManagedObjectCount("tests.MyManagedNativeClass"), 0);
        Assert.equal(Metrics.getManagedObjectCount("tests.MyChildManagedNativeClass"), 0);
        
        // make sure we handle deleting this in callstack
        var testDelete = new MyManagedNativeClass;
        testDelete.testDeleteA();        
    }
}


}



