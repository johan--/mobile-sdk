﻿namespace Carto.Ui {
    using System;
    using UIKit;
    using GLKit;
    using OpenGLES;
    using CoreFoundation;
    using CoreGraphics;
    using Foundation;
    using Carto.Components;
    using Carto.Core;
    using Carto.Layers;
    using Carto.Renderers;
    using Carto.Utils;

    /// <summary>
    /// MapView is an iOS-specific view class supporting map rendering and interaction.
    /// </summary>
    [Foundation.Register("MapView")]
    public partial class MapView : GLKView {
        private bool _contextCreated = false;
        private float _scale = 1;

        private UITouch _pointer1 = null;
        private UITouch _pointer2 = null;

        private class MapViewGLKViewDelegate : GLKViewDelegate {
            public override void DrawInRect(GLKView view, CGRect rect) {
                ((MapView) view).OnDraw();
            }
        }

        static MapView() {
            IOSUtils.InitializeLog();
        }

        /// <summary>
        /// Registers the SDK license. This class method and must be called before
        /// creating any actual MapView instances.
        /// </summary>
        /// <param name="licenseKey">The license string provided for this application.</param>
        /// <returns>True if license is valid, false if not.</returns>
        public static bool RegisterLicense(string licenseKey) {
            ReadKeyDelegate readKey = (string key) => { return NSUserDefaults.StandardUserDefaults.StringForKey(key); };
            WriteKeyDelegate writeKey = (string key, string value) => { NSUserDefaults.StandardUserDefaults.SetString(value, key); NSUserDefaults.StandardUserDefaults.Synchronize(); };
            return RegisterLicenseInternal(licenseKey, readKey, writeKey);
        }

        /// <summary>
        /// Constructs a new MapView object.
        /// </summary>
        /// <param name="rect">The view rectangle.</param>
        public MapView(CGRect rect) : base(rect) {
            InitBase();
        }

        /// <summary>
        /// Constructs a new MapView object.
        /// </summary>
        public MapView() : base() {
            InitBase();
        }

        /// <summary>
        /// Constructs a new MapView object from Interface Builder.
        /// </summary>
        /// <param name="ptr">The Interface Builder context pointer</param>
        public MapView(IntPtr ptr) : base(ptr) {
            InitBase();
        }

        private void InitBase() {
            _scale = (float) UIScreen.MainScreen.Scale;
            _baseMapView = new BaseMapView();
            _baseMapView.SetRedrawRequestListener(new MapRedrawRequestListener(this));
            _baseMapView.GetOptions().DPI = 160 * _scale;

            DispatchQueue.MainQueue.DispatchAsync (
                new System.Action (InitGL)
            );
        }

        private void InitGL() {
            Context = new EAGLContext(EAGLRenderingAPI.OpenGLES2);
            if (Context == null) {
                Log.Fatal("MapView.InitGL: Failed to create OpenGLES 2.0 context");
            }
            _contextCreated = true;

            DrawableColorFormat = GLKViewDrawableColorFormat.RGBA8888;
            DrawableDepthFormat = GLKViewDrawableDepthFormat.Format24;
            DrawableMultisample = GLKViewDrawableMultisample.None;
            DrawableStencilFormat = GLKViewDrawableStencilFormat.Format8;
            MultipleTouchEnabled = true;

            EAGLContext.SetCurrentContext(Context);
            _baseMapView.OnSurfaceCreated();
            _baseMapView.OnSurfaceChanged((int) (Bounds.Size.Width * _scale), (int) (Bounds.Size.Height * _scale));

            this.Delegate = new MapViewGLKViewDelegate();

            SetNeedsDisplay();
        }

        private void OnDraw() {
            if (_contextCreated) {
                EAGLContext.SetCurrentContext(Context);
                _baseMapView.OnDrawFrame();
            }
        }

        protected override void Dispose(bool disposing) {
            if (_contextCreated && EAGLContext.CurrentContext == Context) {
                EAGLContext.SetCurrentContext(null);
                _baseMapView.SetRedrawRequestListener(null);
                _baseMapView = null;
                Context = null;
            }

            base.Dispose(disposing);
        }

        public override void LayoutSubviews() {
            base.LayoutSubviews();

            if (_contextCreated) {
                EAGLContext.SetCurrentContext(Context);
                _baseMapView.OnSurfaceChanged((int) (Bounds.Size.Width * _scale), (int) (Bounds.Size.Height * _scale));
                SetNeedsDisplay();
            }
        }

        public override void TouchesBegan(Foundation.NSSet touches, UIEvent evt) {
            foreach (UITouch pointer in touches) {
                if (_pointer1 == null) {
                    _pointer1 = pointer;
                    CGPoint screenPos1 = _pointer1.LocationInView(this);
                    _baseMapView.OnInputEvent(NativeActionPointer1Down, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, NativeNoCoordinate, NativeNoCoordinate);
                    continue;
                }

                if (_pointer2 == null) {
                    _pointer2 = pointer;
                    CGPoint screenPos1 = _pointer1.LocationInView(this);
                    CGPoint screenPos2 = _pointer2.LocationInView(this);
                    _baseMapView.OnInputEvent(NativeActionPointer2Down, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, (float) screenPos2.X * _scale, (float) screenPos2.Y * _scale);
                    break;
                }
            }
        }

        public override void TouchesMoved(Foundation.NSSet touches, UIEvent evt) {
            if (_pointer1 != null) {
                CGPoint screenPos1 = _pointer1.LocationInView(this);
                if (_pointer2 != null) {
                    CGPoint screenPos2 = _pointer2.LocationInView(this);
                    _baseMapView.OnInputEvent(NativeActionMove, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, (float) screenPos2.X * _scale, (float) screenPos2.Y * _scale);
                } else {
                    _baseMapView.OnInputEvent(NativeActionMove, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, NativeNoCoordinate, NativeNoCoordinate);
                }
            }
        }

        public override void TouchesCancelled(Foundation.NSSet touches, UIEvent evt) {
            // Note: should use ACTION_CANCEL here, but Xamarin.Forms uses this
            // for single clicks, so we need to emulate TouchesEnded here actually
            if (_pointer2 != null && touches.Contains (_pointer2)) {
                if (_pointer1 != null && !touches.Contains(_pointer1)) {
                    CGPoint screenPos1 = _pointer1.LocationInView(this);
                    _baseMapView.OnInputEvent(NativeActionPointer2Up, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, NativeNoCoordinate, NativeNoCoordinate);
                } else {
                    _baseMapView.OnInputEvent(NativeActionPointer2Up, NativeNoCoordinate, NativeNoCoordinate, NativeNoCoordinate, NativeNoCoordinate);
                }
                _pointer2 = null;
            }

            if (_pointer1 != null && touches.Contains(_pointer1)) {
                if (_pointer2 != null) {
                    CGPoint screenPos2 = _pointer2.LocationInView(this);
                    _baseMapView.OnInputEvent(NativeActionPointer1Up, NativeNoCoordinate, NativeNoCoordinate, (float) screenPos2.X * _scale, (float) screenPos2.Y * _scale);
                    _pointer1 = _pointer2;
                    _pointer2 = null;
                } else {
                    _baseMapView.OnInputEvent(NativeActionPointer1Up, NativeNoCoordinate, NativeNoCoordinate, NativeNoCoordinate, NativeNoCoordinate);
                    _pointer1 = null;
                }
            }
        }

        public override void TouchesEnded(Foundation.NSSet touches, UIEvent evt) {
            if (_pointer2 != null && touches.Contains(_pointer2)) {
                CGPoint screenPos1 = _pointer1.LocationInView(this);
                CGPoint screenPos2 = _pointer2.LocationInView(this);
                _baseMapView.OnInputEvent(NativeActionPointer2Up, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, (float) screenPos2.X * _scale, (float) screenPos2.Y * _scale);
                _pointer2 = null;
            }

            if (_pointer1 != null && touches.Contains(_pointer1)) {
                CGPoint screenPos1 = _pointer1.LocationInView(this);
                if (_pointer2 != null) {
                    CGPoint screenPos2 = _pointer2.LocationInView (this);
                    _baseMapView.OnInputEvent(NativeActionPointer1Up, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, (float) screenPos2.X * _scale, (float) screenPos2.Y * _scale);
                    _pointer1 = _pointer2;
                    _pointer2 = null;
                } else {
                    _baseMapView.OnInputEvent(NativeActionPointer1Up, (float) screenPos1.X * _scale, (float) screenPos1.Y * _scale, NativeNoCoordinate, NativeNoCoordinate);
                    _pointer1 = null;
                }
            }
        }
    }
}
