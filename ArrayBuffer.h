#ifndef NODEARRAYBUFFER_H
#define NODEARRAYBUFFER_H

/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 vmolsa <ville.molsa@gmail.com> (http://github.com/vmolsa)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
*/

#include <v8.h>

namespace node {
  template<class T> class ArrayBufferWrapper;
  
  class ArrayBuffer {
    template<class T> friend class ArrayBufferWrapper;
    
   public:
    inline static ArrayBuffer* New(v8::Isolate *isolate = 0, void *ptr = 0, size_t length = 0, bool release = false) {
      ArrayBuffer *data = new ArrayBuffer(isolate);
      v8::Local<v8::ArrayBuffer> arrayBuffer = v8::ArrayBuffer::New(data->_isolate, ptr, length);

      data->_rel = release;
      data->_len = length;
      data->_data = ptr;
      data->_arrayBuffer.Reset(isolate, arrayBuffer);
      data->_arrayBuffer.SetWeak(data, ArrayBuffer::onDispose);
      data->_arrayBuffer.MarkIndependent();

      arrayBuffer->SetHiddenValue(v8::String::NewFromUtf8(data->_isolate, "node::ArrayBuffer"), 
                                  v8::External::New(data->_isolate, data));

      return data;
    }
    
    inline static ArrayBuffer* New(v8::Isolate *isolate, const char *ptr, int length = -1, bool release = false) {
      const void *data = ptr;
      
      if (length < 0) {        
        for (length = 0; ptr && (ptr[length] || ptr[length] != '\0'); length++) { }
      }
      
      return ArrayBuffer::New(isolate, const_cast<void*>(data), length, release);
    }
    
    inline static ArrayBuffer* New(v8::Isolate *isolate, v8::Local<v8::ArrayBuffer> arrayBuffer) {
      if (arrayBuffer.IsEmpty()) {
        return ArrayBuffer::New(isolate);
      }

      if (arrayBuffer->IsExternal()) {
        v8::Local<v8::Value> ptr = arrayBuffer->GetHiddenValue(v8::String::NewFromUtf8(isolate, "node::ArrayBuffer"));

        if (ptr.IsEmpty()) {
          v8::ArrayBuffer::Contents content = arrayBuffer->Externalize();
          return ArrayBuffer::New(isolate, content.Data(), content.ByteLength());
        } else {          
          v8::Local<v8::External> ext = v8::Local<v8::External>::Cast(ptr);
          return static_cast<ArrayBuffer*>(ext->Value());
        }
      } else {        
        ArrayBuffer *data = new ArrayBuffer(isolate);
        v8::ArrayBuffer::Contents content = arrayBuffer->Externalize();
        
        data->_rel = true;
        data->_len = content.ByteLength();
        data->_data = content.Data();
        data->_arrayBuffer.Reset(data->_isolate, arrayBuffer);
        data->_arrayBuffer.SetWeak(data, ArrayBuffer::onDispose);
        data->_arrayBuffer.MarkIndependent();

        arrayBuffer->SetHiddenValue(v8::String::NewFromUtf8(data->_isolate, "node::ArrayBuffer"), 
                                    v8::External::New(data->_isolate, data));

        return data;
      }

      return 0;
    }
    
    template<class T>
    inline static ArrayBuffer* New(v8::Isolate *isolate, const T &content) {
      return ArrayBuffer::New(isolate, content, content.data(), content.size());
    }
    
    template<class T>
    inline static ArrayBuffer* New(v8::Isolate *isolate, const T &content, void *ptr, size_t length = 0) {
      ArrayBufferWrapper<T> *ret = new ArrayBufferWrapper<T>(isolate, content, ptr, length);
      return static_cast<ArrayBuffer*>(ret);
    }   

    template<class T> 
    inline static ArrayBuffer* New(v8::Isolate *isolate, const T &content, const char *ptr, int length = -1) {
      const void *data = ptr;

      if (length < 0) {        
        for (length = 0; ptr && (ptr[length] || ptr[length] != '\0'); length++) { }
      }

      ArrayBufferWrapper<T> *ret = new ArrayBufferWrapper<T>(isolate, content, const_cast<void*>(data), length);
      return static_cast<ArrayBuffer*>(ret);
    }    
     
    inline v8::Local<v8::ArrayBuffer> ToArrayBuffer() const {
      v8::EscapableHandleScope scope(_isolate);
      return scope.Escape(v8::Local<v8::ArrayBuffer>::New(_isolate, _arrayBuffer));
    }
    
    v8::Local<v8::String> ToString() const {
      v8::EscapableHandleScope scope(_isolate);
      v8::Local<v8::String> retval = v8::String::NewFromUtf8(_isolate, 
                                                             ArrayBuffer::String(),
                                                             v8::String::kNormalString,
                                                             ArrayBuffer::Length());
      return scope.Escape(retval);
    }
    
    const char *String() const {
      return reinterpret_cast<const char*>(_data);
    }
    
    void *Data() const {
      return _data;
    }
    
    size_t Length() const {
      return _len;
    }
    
    size_t ByteLength() const { 
      return _len;
    }
    
    template<class T> const T &Unwrap() const {
      static T nowrap;
      
      if (_wrap) {
        const ArrayBufferWrapper<T> *ptr = static_cast<const ArrayBufferWrapper<T>*>(this);
        return ptr->_content;
      }
      
      nowrap = T();
      return nowrap;
    }
    
   private:
    explicit ArrayBuffer(v8::Isolate *isolate = 0) : 
      _rel(false),
      _wrap(false),
      _len(0),
      _data(0),
      _isolate(isolate)
    {
      if (!_isolate) {
        _isolate = v8::Isolate::GetCurrent();
      }
    }
    
    virtual ~ArrayBuffer() {
      if (_rel) {
        delete [] reinterpret_cast<char*>(_data);
      }
    }
    
    static void onDispose(const v8::WeakCallbackData<v8::ArrayBuffer, ArrayBuffer> &info) {      
      v8::Isolate *isolate = info.GetIsolate();
      v8::HandleScope scope(isolate);
      
      ArrayBuffer *wrap = info.GetParameter();
      
      v8::Local<v8::ArrayBuffer> arrayBuffer = v8::Local<v8::ArrayBuffer>::New(isolate, wrap->_arrayBuffer);
      wrap->_arrayBuffer.Reset();
      
      if (!arrayBuffer.IsEmpty()) {
        arrayBuffer->DeleteHiddenValue(v8::String::NewFromUtf8(isolate, "node::ArrayBuffer"));
      }
      
      delete wrap;
    }
    
   protected:
    bool _rel;
    bool _wrap;
    size_t _len;
    void* _data;
    v8::Isolate* _isolate;
    v8::Persistent<v8::ArrayBuffer> _arrayBuffer;
  };
  
  template<class T> class ArrayBufferWrapper : public ArrayBuffer {
    friend class ArrayBuffer;

   private:
    explicit ArrayBufferWrapper(v8::Isolate *isolate, const T &content, void *ptr = 0, size_t length = 0) :
      ArrayBuffer(isolate),
      _content(content)
    {
      v8::Local<v8::ArrayBuffer> arrayBuffer = v8::ArrayBuffer::New(_isolate, ptr, length);

      _wrap = true;
      _len = length;
      _data = ptr;
      _arrayBuffer.Reset(_isolate, arrayBuffer);
      _arrayBuffer.SetWeak(static_cast<ArrayBuffer*>(this), ArrayBuffer::onDispose);
      _arrayBuffer.MarkIndependent();

      arrayBuffer->SetHiddenValue(v8::String::NewFromUtf8(_isolate, "node::ArrayBuffer"), 
                                  v8::External::New(_isolate, this));
    }

    virtual ~ArrayBufferWrapper() { }

   protected:
    T _content;
  };  
};

#endif
