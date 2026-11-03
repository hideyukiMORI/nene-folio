#include "image_decode.h"
#include <stdlib.h>
#include <wincodec.h>
#include <windows.h>
struct image_decode
{
    IWICImagingFactory *_Nullable factory;
    IWICStream *_Nullable stream;
    IWICBitmapDecoder *_Nullable decoder;
    IWICBitmapFrameDecode *_Nullable frame;
    IWICBitmapScaler *_Nullable scaler;
    IWICFormatConverter *_Nullable converter;
    unsigned char *_Nullable pixels;
};
static void image_decode_destroy(struct image_decode *_Nonnull decoder)
{
    free(decoder->pixels);
    if (decoder->converter != nullptr)
        decoder->converter->lpVtbl->Release(decoder->converter);
    if (decoder->scaler != nullptr)
        decoder->scaler->lpVtbl->Release(decoder->scaler);
    if (decoder->frame != nullptr)
        decoder->frame->lpVtbl->Release(decoder->frame);
    if (decoder->decoder != nullptr)
        decoder->decoder->lpVtbl->Release(decoder->decoder);
    if (decoder->stream != nullptr)
        decoder->stream->lpVtbl->Release(decoder->stream);
    if (decoder->factory != nullptr)
        decoder->factory->lpVtbl->Release(decoder->factory);
}
static bool open_decoder(struct image_decode *_Nonnull decoder, struct image_encoded encoded)
{
    void *_Nullable factory = nullptr;
    HRESULT created = CoCreateInstance(&CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       &IID_IWICImagingFactory, &factory);
    if (FAILED(created) || factory == nullptr)
        return false;
    decoder->factory = factory;
    if (FAILED(decoder->factory->lpVtbl->CreateStream(decoder->factory, &decoder->stream)))
        return false;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, encoded.length);
    if (memory == nullptr)
        return false;
    void *_Nullable writable = GlobalLock(memory);
    if (writable == nullptr)
    {
        GlobalFree(memory);
        return false;
    }
    memcpy(writable, encoded.bytes, encoded.length);
    GlobalUnlock(memory);
    IStream *_Nullable stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)) || stream == nullptr)
    {
        GlobalFree(memory);
        return false;
    }
    HRESULT initialized = decoder->stream->lpVtbl->InitializeFromIStream(decoder->stream, stream);
    stream->lpVtbl->Release(stream);
    return SUCCEEDED(initialized) && SUCCEEDED(decoder->factory->lpVtbl->CreateDecoderFromStream(
                                         decoder->factory, (IStream *)decoder->stream, nullptr,
                                         WICDecodeMetadataCacheOnDemand, &decoder->decoder));
}
static enum image_outcome prepare_frame(struct image_decode *_Nonnull decoder,
                                        struct image_size *_Nonnull size)
{
    GUID format;
    if (FAILED(decoder->decoder->lpVtbl->GetContainerFormat(decoder->decoder, &format)))
        return IMAGE_FAILED;
    if (!IsEqualGUID(&format, &GUID_ContainerFormatPng) &&
        !IsEqualGUID(&format, &GUID_ContainerFormatJpeg))
        return IMAGE_UNSUPPORTED;
    if (FAILED(decoder->decoder->lpVtbl->GetFrame(decoder->decoder, 0, &decoder->frame)) ||
        FAILED(decoder->frame->lpVtbl->GetSize(decoder->frame, &size->width, &size->height)))
        return IMAGE_FAILED;
    if (size->width == 0 || size->height == 0 ||
        (uint64_t)size->width * size->height > image_input_pixel_limit)
        return IMAGE_TOO_LARGE;
    return IMAGE_READY;
}
static void constrain(struct image_size *_Nonnull size, const struct image_work *_Nonnull work)
{
    uint32_t longest = size->width > size->height ? size->width : size->height;
    constexpr uint32_t decoded_edge_limit = 2048;
    if (longest <= decoded_edge_limit)
        return;
    work->progress(work, IMAGE_PHASE_RESIZING);
    size->width = (uint32_t)((uint64_t)size->width * decoded_edge_limit / longest);
    size->height = (uint32_t)((uint64_t)size->height * decoded_edge_limit / longest);
    if (size->width == 0)
        size->width = 1;
    if (size->height == 0)
        size->height = 1;
}
static enum image_outcome decode_pixels(struct image_decode *_Nonnull decoder,
                                        struct image_size size,
                                        struct image_bitmap *_Nullable *_Nonnull out)
{
    if (FAILED(decoder->factory->lpVtbl->CreateBitmapScaler(decoder->factory, &decoder->scaler)) ||
        FAILED(decoder->scaler->lpVtbl->Initialize(decoder->scaler,
                                                   (IWICBitmapSource *)decoder->frame, size.width,
                                                   size.height, WICBitmapInterpolationModeFant)) ||
        FAILED(decoder->factory->lpVtbl->CreateFormatConverter(decoder->factory,
                                                               &decoder->converter)) ||
        FAILED(decoder->converter->lpVtbl->Initialize(
            decoder->converter, (IWICBitmapSource *)decoder->scaler, &GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
        return IMAGE_FAILED;
    size_t length = (size_t)size.width * size.height * 4;
    decoder->pixels = malloc(length);
    if (decoder->pixels == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    if (FAILED(decoder->converter->lpVtbl->CopyPixels(decoder->converter, nullptr, size.width * 4,
                                                      (UINT)length, decoder->pixels)))
        return IMAGE_FAILED;
    return image_bitmap_create(decoder->pixels, size, out);
}
enum image_outcome image_decode_read(struct image_encoded encoded,
                                     const struct image_work *_Nonnull work,
                                     struct image_bitmap *_Nullable *_Nonnull out)
{
    if (encoded.length == 0 || encoded.length > image_input_byte_limit)
        return IMAGE_TOO_LARGE;
    if (work->cancelled(work))
        return IMAGE_CANCELLED;
    work->progress(work, IMAGE_PHASE_DECODING);
    struct image_decode decoder = {0};
    enum image_outcome outcome = IMAGE_FAILED;
    struct image_size size = {0};
    if (open_decoder(&decoder, encoded))
        outcome = prepare_frame(&decoder, &size);
    if (outcome == IMAGE_READY)
    {
        constrain(&size, work);
        outcome = work->cancelled(work) ? IMAGE_CANCELLED : decode_pixels(&decoder, size, out);
    }
    image_decode_destroy(&decoder);
    return outcome;
}
