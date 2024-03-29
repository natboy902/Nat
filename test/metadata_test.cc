/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "aom/aom_codec.h"
#include "aom/aom_image.h"
#include "aom/internal/aom_image_internal.h"
#include "aom_scale/yv12config.h"
#include "av1/encoder/bitstream.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {
const size_t kExampleDataSize = 10;
const uint8_t kExampleData[kExampleDataSize] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

#if CONFIG_AV1_ENCODER

class MetadataEncodeTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  MetadataEncodeTest() : EncoderTest(GET_PARAM(0)) {}

  virtual ~MetadataEncodeTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video) {
    aom_image_t *current_frame = video->img();
    if (current_frame) {
      if (current_frame->metadata) aom_img_remove_metadata(current_frame);
      ASSERT_EQ(aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_ITUT_T35,
                                     kExampleData, kExampleDataSize),
                0);
    }
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const size_t metadata_obu_size = 14;
      const uint8_t metadata_obu[metadata_obu_size] = {
        42, 12, 4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128
      };
      const size_t bitstream_size = pkt->data.frame.sz;
      const uint8_t *bitstream =
          static_cast<const uint8_t *>(pkt->data.frame.buf);
      // look for expected metadata in bitstream
      bool found_metadata = false;
      for (size_t i = 0; i < bitstream_size; ++i) {
        if (memcmp(bitstream + i, metadata_obu, metadata_obu_size) == 0) {
          found_metadata = true;
          break;
        }
      }
      EXPECT_TRUE(found_metadata);
    }
  }
};

TEST_P(MetadataEncodeTest, TestMetadataEncoding) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 5);
  init_flags_ = AOM_CODEC_USE_PSNR;

  cfg_.g_w = 352;
  cfg_.g_h = 288;

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 600;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_undershoot_pct = 50;
  cfg_.rc_overshoot_pct = 50;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.kf_mode = AOM_KF_AUTO;
  cfg_.g_lag_in_frames = 1;
  cfg_.kf_min_dist = cfg_.kf_max_dist = 3000;
  // Enable dropped frames.
  cfg_.rc_dropframe_thresh = 1;
  // Disable error_resilience mode.
  cfg_.g_error_resilient = 0;
  // Run at low bitrate.
  cfg_.rc_target_bitrate = 40;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_CASE(MetadataEncodeTest,
                          ::testing::Values(::libaom_test::kOnePassGood));

#endif  // CONFIG_AV1_ENCODER
}  // namespace

TEST(MetadataTest, MetadataAllocation) {
  aom_metadata_t *metadata = aom_img_metadata_alloc(
      OBU_METADATA_TYPE_ITUT_T35, kExampleData, kExampleDataSize);
  ASSERT_NE(metadata, nullptr);
  aom_img_metadata_free(metadata);
}

TEST(MetadataTest, MetadataArrayAllocation) {
  aom_metadata_array_t *metadata_array = aom_img_metadata_array_alloc(2);
  ASSERT_NE(metadata_array, nullptr);

  metadata_array->metadata_array[0] = aom_img_metadata_alloc(
      OBU_METADATA_TYPE_ITUT_T35, kExampleData, kExampleDataSize);
  metadata_array->metadata_array[1] = aom_img_metadata_alloc(
      OBU_METADATA_TYPE_ITUT_T35, kExampleData, kExampleDataSize);

  aom_img_metadata_array_free(metadata_array);
}

TEST(MetadataTest, AddMetadataToImage) {
  aom_image_t image;
  image.metadata = NULL;

  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35,
                                 kExampleData, kExampleDataSize),
            0);
  aom_img_metadata_array_free(image.metadata);
  EXPECT_EQ(aom_img_add_metadata(NULL, OBU_METADATA_TYPE_ITUT_T35, kExampleData,
                                 kExampleDataSize),
            -1);
}

TEST(MetadataTest, RemoveMetadataFromImage) {
  aom_image_t image;
  image.metadata = NULL;

  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35,
                                 kExampleData, kExampleDataSize),
            0);
  aom_img_remove_metadata(&image);
  aom_img_remove_metadata(NULL);
}

TEST(MetadataTest, CopyMetadataToFrameBuffer) {
  YV12_BUFFER_CONFIG yvBuf;
  yvBuf.metadata = NULL;

  aom_metadata_array_t *metadata_array = aom_img_metadata_array_alloc(1);
  ASSERT_NE(metadata_array, nullptr);

  metadata_array->metadata_array[0] = aom_img_metadata_alloc(
      OBU_METADATA_TYPE_ITUT_T35, kExampleData, kExampleDataSize);

  // Metadata_array
  int status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array);
  EXPECT_EQ(status, 0);
  status = aom_copy_metadata_to_frame_buffer(NULL, metadata_array);
  EXPECT_EQ(status, -1);
  aom_img_metadata_array_free(metadata_array);

  // Metadata_array_2
  aom_metadata_array_t *metadata_array_2 = aom_img_metadata_array_alloc(0);
  ASSERT_NE(metadata_array_2, nullptr);
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array_2);
  EXPECT_EQ(status, -1);
  aom_img_metadata_array_free(metadata_array_2);

  // YV12_BUFFER_CONFIG
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, NULL);
  EXPECT_EQ(status, -1);
  aom_remove_metadata_from_frame_buffer(&yvBuf);
  aom_remove_metadata_from_frame_buffer(NULL);
}
