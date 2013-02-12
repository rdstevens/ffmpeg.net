#pragma once


#include "ffmpeg_actual.h"

#include "PixelFormat.h"


using namespace System;

namespace FFMpegNet 
{
	namespace AVCodec
	{
		public ref class AVFrame
		{
		
		protected:
			AVFrame() : refCount(0)
			{
				objCount++;
			}


			bool manageMemory;

			// TODO - COPY CONSTRUCTOR!!
		public:
			AVFrame(::AVFrame* _avFrame)
			{
				if (_avFrame == 0)
				{
					throw gcnew ArgumentNullException("Cannot create an AVFrame with a null reference");
				}
				this->_avFrame = _avFrame;
			}
		
			~AVFrame()
			{

				if (refCount > 0)
				{
					throw gcnew Exception("Memory leak: You're disposing of an AVframe but references still exist");
				}
			}


			static AVFrame()
			{
				objCount = 0;
			}

			static int objCount;

			int refCount;
			void AddRef()
			{
				refCount++;
				//System::Console::WriteLine("RefCount++ now {0} on {1}", refCount, GetHashCode());
			}

			void Release()
			{
				refCount--;
				//System::Console::WriteLine("RefCount-- now {0} on {1}", refCount, GetHashCode());
				if (refCount <= 0)
				{
					if (_avFrame != NULL && manageMemory)
					{
						//System::Console::WriteLine("De-allocating AVFrame");
						if (_avFrame->type == static_cast<::AVMediaType>(AVUtil::AVMediaType::VIDEO))
						{
							::avpicture_free((::AVPicture*)_avFrame);
						}
						else if (_avFrame->type == static_cast<::AVMediaType>(AVUtil::AVMediaType::AUDIO))
						{
							free(_avFrame->data[0]);
							_avFrame->data[0] = 0;
							_avFrame->linesize[0] = 0;
						}
						else
						{
							throw gcnew NotImplementedException();
						}
						::av_free(_avFrame);
						_avFrame = NULL;

						objCount--;
					}
				}
			}

			// Type of value will actually depend on sample format. Templated type?
			void SetSample(int sampleNumber, short value)
			{
				_avFrame->data[0][2* sampleNumber] = (uint8_t)value;
				_avFrame->data[0][2* sampleNumber + 1] = (uint8_t)value;
			}

			void SetType(int type)
			{
				_avFrame->type = type;
			}

			property __int64 Pts
			{
				__int64 get()
				{
					return _avFrame->pts;
				}
				void set(__int64 value)
				{
					_avFrame->pts = value;
				}
			}


			property bool KeyFrame
			{
				bool get()
				{
					return (bool)(_avFrame->key_frame == 0);
				}
			}


			// TODO: Sort out better data type. IntPtr probably...
			property int Data
			{
				int get ()
				{
					return (int) _avFrame->data[0];
				}
			}

			property int Size
			{
				int get()
				{
					int totalSize = 0;
					for(int i = 0 ; i < AV_NUM_DATA_POINTERS; i++)
					{
						totalSize += _avFrame->linesize[i];
					}
					return totalSize;
				}
			}

			property int CodedFrameNumber
			{
				int get()
				{
					return _avFrame->coded_picture_number;
				}
			}

		internal:
			::AVFrame * _avFrame;
		};

		public ref class AVVideoFrame : AVFrame
		{
		public:
			AVVideoFrame(AVUtil::PixelFormat pixelFormat, int width, int height) : AVFrame()
			{
				::PixelFormat avPixelFormat = static_cast<::PixelFormat>(pixelFormat);
				_avFrame = ::avcodec_alloc_frame();
				::avpicture_alloc((::AVPicture *)_avFrame, avPixelFormat, width, height);
				_avFrame->width = width;
				_avFrame->height = height;
				_avFrame->format = avPixelFormat;
				_avFrame->extended_data = _avFrame->data;
				
				_avFrame->type = static_cast<::PixelFormat>(FFMpegNet::AVUtil::AVMediaType::VIDEO);
				manageMemory = true;
			};

			AVVideoFrame(AVUtil::PixelFormat pixelFormat, int width, int height, IntPtr^ p) : AVFrame()
			{
				::PixelFormat avPixelFormat = static_cast<::PixelFormat>(pixelFormat);
				_avFrame = ::avcodec_alloc_frame();
				int size = ::avpicture_get_size(avPixelFormat, width, height);
				::avpicture_fill((::AVPicture *)_avFrame, static_cast<uint8_t *>(p->ToPointer()), avPixelFormat, width, height);

				_avFrame->type = static_cast<::PixelFormat>(FFMpegNet::AVUtil::AVMediaType::VIDEO);
				manageMemory = false;
			}

			// Horribly, horribly slow.
			void SetPixel(int plane, int x, int y, int argb)
			{
				_avFrame->data[plane][y * _avFrame->linesize[plane] + 3*x] = argb >> 24;
				_avFrame->data[plane][y * _avFrame->linesize[plane] + 3*x + 1] = argb >> 16;
				_avFrame->data[plane][y * _avFrame->linesize[plane] + 3*x + 2] = argb >> 8;
			}

			property int Width
			{
				int get()
				{
					return _avFrame->width;
				}
			}

			property int Height
			{
				int get()
				{
					return _avFrame->height;
				}
			}

			property AVUtil::PixelFormat PixelFormat
			{
				AVUtil::PixelFormat get()
				{
					return static_cast<AVUtil::PixelFormat>(_avFrame->format);
				}
			}
		};

		public ref class AVAudioFrame : AVFrame
		{
		public:
			AVAudioFrame(AVCodec::AVSampleFormat sampleFormat, int numberOfSamples, int channels, int sampleRate) : AVFrame()
			{
				int audioBufferLength = Init(sampleFormat, numberOfSamples, channels, sampleRate);

				manageMemory = true;
				
				_avFrame->data[0] = (uint8_t *)malloc(audioBufferLength);
				_avFrame->extended_data = _avFrame->data;	// to avoid warning
			}

			AVAudioFrame(AVCodec::AVSampleFormat sampleFormat, int numberOfSamples, int channels, int sampleRate, IntPtr^ p) : AVFrame()
			{
				int audioBufferLength = Init(sampleFormat, numberOfSamples, channels, sampleRate);
				
				manageMemory = false;

				_avFrame->data[0] = static_cast<uint8_t *> (p->ToPointer());
				_avFrame->extended_data = _avFrame->data;	// to avoid warning
			}

		private:
			int Init(AVCodec::AVSampleFormat sampleFormat, int numberOfSamples, int channels, int sampleRate)
			{
				if (numberOfSamples <= 0)
				{
					throw gcnew ArgumentOutOfRangeException("numberOfSamples must be >0");
				}
				if (channels <= 0)
				{
					throw gcnew ArgumentOutOfRangeException("channels must be >0");
				}
				if (sampleRate <= 0)
				{
					throw gcnew ArgumentOutOfRangeException("sampleRate must be >0");
				}

				_avFrame = ::avcodec_alloc_frame();
				_avFrame->format = static_cast<::AVSampleFormat>(sampleFormat);
				_avFrame->nb_samples = numberOfSamples;

				int bytesPerSample = ::av_get_bytes_per_sample((::AVSampleFormat)_avFrame->format);
				int audioSampleLength = bytesPerSample * channels;	
				int audioBufferLength = audioSampleLength * _avFrame->nb_samples;

				_avFrame->linesize[0] = audioBufferLength;
				_avFrame->sample_rate = sampleRate;
				_avFrame->channels = channels;

				_avFrame->type = static_cast<::AVMediaType>(FFMpegNet::AVUtil::AVMediaType::AUDIO);			

				return audioBufferLength;
			}
		public:

			property int NumberOfSamples
			{
				int get()
				{
					return _avFrame->nb_samples;
				}
			}

			property int SampleRate
			{
				int get()
				{
					return _avFrame->sample_rate;
				}
			}

			property int Channels
			{
				int get()
				{
					return (int) _avFrame->channels;
				}
			}



			property FFMpegNet::AVCodec::AVSampleFormat SampleFormat
			{
				FFMpegNet::AVCodec::AVSampleFormat get()
				{
					return static_cast<FFMpegNet::AVCodec::AVSampleFormat>(_avFrame->format);
				}
			}
		};

	};
};
