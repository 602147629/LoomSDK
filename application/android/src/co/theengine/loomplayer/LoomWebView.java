package co.theengine.loomplayer;

import java.util.Hashtable;
import java.util.Set;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebChromeClient;
import android.widget.RelativeLayout;

/**
 *	Java class that manages WebView instances. This maps directly to the platformWebView C API
 */
public class LoomWebView {

	private static native void nativeCallback(String data, long callback, long payload, int type);

    private static void deferNativeCallback(String data, long callback, long payload, int type)
    {
        final String fData = data;
        final long fCallback = callback;
        final long fPayload = payload;
        final int fType = type;

        // TODO: does this require queueEvent?
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                nativeCallback(fData, fCallback, fPayload, fType);
            }
        });
    }

	public static int create(final long callback, final long payload)
	{
		final int handle = webViewCounter++;
		final Context context = activity;
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = new WebView(context);
				
				LoomWebView.webViews.put(handle, webView);
				
				webView.setWebChromeClient(new WebChromeClient());
				webView.setWebViewClient(new WebViewClient() {
					@Override
					public void onReceivedError(WebView view, int errorCode,
							String description, String failingUrl) {
						super.onReceivedError(view, errorCode, description, failingUrl);
						deferNativeCallback(description, callback, payload, 1);
					}
					
					@Override
					public boolean shouldOverrideUrlLoading(WebView view, String url) {
						return false;
					}
					
					@Override
					public void onPageStarted(WebView view, String url,
							Bitmap favicon) {
						super.onPageStarted(view, url, favicon);
						deferNativeCallback(url, callback, payload, 0);
					}
					
				});

				webView.getSettings().setJavaScriptEnabled(true);
				webView.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
			}
		});
		
		return handle;
	}
	
	public static void show(final int handle)
	{
		final ViewGroup layout = rootLayout;
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				layout.addView(webView);
				RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(webView.getLayoutParams());
				params.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
				webView.setLayoutParams(params);
			}
		});
	}
	
	public static void hide(final int handle)
	{
		final ViewGroup layout = rootLayout;
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				
				if(webView.getParent() == layout)
					layout.removeView(webView);
			}
		});
	}
	
	public static void destroy(final int handle)
	{
		final ViewGroup layout = rootLayout;
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = webViews.get(handle);
				LoomWebView.webViews.remove(handle);
				
				if(webView != null && webView.getParent() == layout)
					layout.removeView(webView);
			}
		});
	}
	
	public static void destroyAll()
	{
		final ViewGroup layout = rootLayout;
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				Set<Integer> keys = webViews.keySet();
				for(Integer key: keys){
					WebView webView = LoomWebView.webViews.remove(key);
					
					if(webView != null && webView.getParent() == layout)
						layout.removeView(webView);
		        }
			}
		});
	}
	
	public static void request(final int handle, final String url) 
	{	
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				webView.loadUrl(url);
			}
		});
	}
	
	public static boolean goBack(final int handle)
	{	
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				
				if(webView.canGoBack()) {
					payload.boolValue = true;
					webView.goBack();
				}
				else {
					payload.boolValue = false;
				}
				
				synchronized (payload) {				
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.boolValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return false;
	}
	
	public static boolean goForward(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				
				if(webView.canGoForward()) {
					payload.boolValue = true;
					webView.goForward();
				}
				else {
					payload.boolValue = false;
				}
				
				synchronized (payload) {				
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.boolValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return false;
	}
	
	public static boolean canGoBack(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				
				payload.boolValue = webView.canGoBack();
				
				synchronized (payload) {
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.boolValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return false;
	}
	
	public static boolean canGoForward(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				
				payload.boolValue = webView.canGoForward();
				
				synchronized (payload) {				
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.boolValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return false;
	}
	
	public static void setDimensions(final int handle, final int x, final int y, final int width, final int height)
	{
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
				{
					params.bottomMargin = y;
					params.leftMargin = x;
					params.width = width;
					params.height = height;
					webView.setLayoutParams(params);
				}
				
			}
		});
	}
	
	public static int getX(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
					payload.intValue = params.leftMargin;
				synchronized (payload) {
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.intValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return -1;
	}
	
	public static void setX(final int handle, final int x)
	{
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
				{
					params.leftMargin = x;
					webView.setLayoutParams(params);
				}
				
			}
		});
	}
	
	public static int getY(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
					payload.intValue = params.bottomMargin;
				synchronized (payload) {
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.intValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return -1;
	}
	
	public static void setY(final int handle, final int y)
	{
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
				{
					params.bottomMargin = y;
					webView.setLayoutParams(params);
				}
			}
		});
	}
	
	public static int getWidth(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
					payload.intValue = params.width;
				synchronized (payload) {
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.intValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return -1;
	}
	
	public static void setWidth(final int handle, final int width)
	{
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
				{
					params.width = width;
					webView.setLayoutParams(params);
				}
			}
		});
	}
	
	public static int getHeight(final int handle)
	{
		final LoomWebViewPayload payload = new LoomWebViewPayload();
		
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
					payload.intValue = params.height;
				synchronized (payload) {
					payload.notify();
				}
			}
		});
		
		try {
			synchronized (payload) {
				payload.wait();
			}
			return payload.intValue;
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		return -1;
	}
	
	public static void setHeight(final int handle, final int height)
	{
		activity.runOnUiThread(new Runnable() {
			
			@Override
			public void run() {
				WebView webView = LoomWebView.webViews.get(handle);
				RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams)webView.getLayoutParams();
				if(params != null)
				{
					params.height = height;
					webView.setLayoutParams(params);
				}		
			}
		});
	}
	
	public static void setRootLayout(ViewGroup value)
	{
		rootLayout = value;
		activity = (Activity)rootLayout.getContext();
	}
	
	protected static int webViewCounter = 0;
	protected static ViewGroup rootLayout;
	protected static Activity activity;
	protected static Hashtable<Integer, WebView> webViews = new Hashtable<Integer, WebView>();
}

class LoomWebViewPayload
{
	public Boolean boolValue;
	public int intValue;
}