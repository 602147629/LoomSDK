package co.theengine.loomdemo;

import android.app.Activity;
import android.util.Log;
import android.os.Bundle;
import android.content.Intent;
import android.content.Context;

import com.CarrotInc.Carrot.Carrot;


public class LoomTeak 
{
    private static final String TAG = "LoomTeak";
    private static final String TEAK_SECRET_KEY = "com.teak.AppSecret";

    static Carrot mTeak = null;

    protected static Activity activity;


    public static void onCreate(LoomDemo loomDemo, String facebookAppId) 
    {
        activity = LoomAdMob.activity;
        if(mTeak == null) 
        {
            String teakKey = LoomDemo.getMetadataString(loomDemo, TEAK_SECRET_KEY);
            // Log.d(TAG, "Initialize Teak... TeakKey: " + teakKey);

            if((teakKey != null) && !teakKey.isEmpty() && !teakKey.trim().isEmpty() &&
                (facebookAppId != null) && !facebookAppId.isEmpty() && !facebookAppId.trim().isEmpty()) 
            {
                mTeak = new Carrot(loomDemo, facebookAppId, teakKey);
                mTeak.setHandler(new Carrot.Handler() 
                {
                    @Override
                    public void authenticationStatusChanged(int authStatus) 
                    {
                        final int _authStatus = authStatus;
                        // TODO: does this require queueEvent?
                        activity.runOnUiThread(new Runnable() 
                        {
                            @Override
                            public void run() 
                            {
                                authStatusCallback(_authStatus);
                            }
                        });
                    }
                });
            }
        }
    }


    public static void onDestroy() 
    {
        if(mTeak != null) 
        {
            mTeak.close();
            mTeak = null;
        }
    }


    public static boolean isActive() 
    {
        return (mTeak == null) ? false : true;
    }

    public static void setAccessToken(String accessToken) 
    {
        if(mTeak != null) 
        {
            mTeak.setAccessToken(accessToken);
        }
    }

    public static int getStatus() 
    {
        if(mTeak != null) 
        {
            return mTeak.getStatus();
        }
        return -1;
    }

    public static boolean postAchievement(String achievementId) 
    {
        if(mTeak != null) 
        {
            return mTeak.postAchievement(achievementId);
        }
        return false;
    }

    public static boolean postHighScore(int score) 
    {
        if(mTeak != null) 
        {
            return mTeak.postHighScore(score);
        }
        return false;
    }

    public static boolean postAction(String actionId, String objectInstanceId) 
    {
        if(mTeak != null) 
        {
            return mTeak.postAction(actionId, objectInstanceId);
        }
        return false;
    }

    public static boolean postActionWithProperties(String actionId, String objectInstanceId, String jsonProperties)
    {
        if(mTeak != null) 
        {
            return mTeak.postJsonAction(actionId, jsonProperties, objectInstanceId);
        }
        return false;
    }


    private static native void authStatusCallback(int authStatus);
}
