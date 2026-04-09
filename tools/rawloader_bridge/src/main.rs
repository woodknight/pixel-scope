use std::env;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::Path;

const MAGIC: &[u8; 8] = b"PSRDNG1\0";

fn write_u16_le(writer: &mut dyn Write, value: u16) -> Result<(), String> {
    writer.write_all(&value.to_le_bytes()).map_err(|err| err.to_string())
}

fn write_u32_le(writer: &mut dyn Write, value: u32) -> Result<(), String> {
    writer.write_all(&value.to_le_bytes()).map_err(|err| err.to_string())
}

fn write_u64_le(writer: &mut dyn Write, value: u64) -> Result<(), String> {
    writer.write_all(&value.to_le_bytes()).map_err(|err| err.to_string())
}

fn write_i32_le(writer: &mut dyn Write, value: i32) -> Result<(), String> {
    writer.write_all(&value.to_le_bytes()).map_err(|err| err.to_string())
}

fn bits_for_max_value(value: u16) -> u32 {
    if value == 0 {
        16
    } else {
        u16::BITS - value.leading_zeros()
    }
}

fn derive_original_bits(samples: &[u16], whitelevels: [u16; 4]) -> u32 {
    let max_white = whitelevels.into_iter().max().unwrap_or(0);
    let max_sample = samples.iter().copied().max().unwrap_or(0);
    bits_for_max_value(max_white.max(max_sample))
}

fn cfa_pattern(image: &rawloader::RawImage) -> [i32; 4] {
    if image.cpp != 1 || !image.cfa.is_valid() || image.cfa.width != 2 || image.cfa.height != 2 {
        return [-1, -1, -1, -1];
    }

    [
        image.cfa.color_at(0, 0) as i32,
        image.cfa.color_at(0, 1) as i32,
        image.cfa.color_at(1, 0) as i32,
        image.cfa.color_at(1, 1) as i32,
    ]
}

fn integer_samples(image: &rawloader::RawImage) -> Result<Vec<u16>, String> {
    match &image.data {
        rawloader::RawImageData::Integer(samples) => Ok(samples.clone()),
        rawloader::RawImageData::Float(_) => {
            Err("rawloader returned a floating-point payload, which PixelScope does not support yet."
                .to_string())
        }
    }
}

fn write_payload(image: &rawloader::RawImage, output_path: &Path) -> Result<(), String> {
    let samples = integer_samples(image)?;
    let width = u32::try_from(image.width).map_err(|_| "Image width exceeds u32 range.".to_string())?;
    let height = u32::try_from(image.height).map_err(|_| "Image height exceeds u32 range.".to_string())?;
    let cpp =
        u32::try_from(image.cpp).map_err(|_| "Channel count exceeds u32 range.".to_string())?;
    let original_bits = derive_original_bits(&samples, image.whitelevels);
    let sample_count =
        u64::try_from(samples.len()).map_err(|_| "Sample count exceeds u64 range.".to_string())?;

    let file = File::create(output_path).map_err(|err| err.to_string())?;
    let mut writer = BufWriter::new(file);
    writer.write_all(MAGIC).map_err(|err| err.to_string())?;
    write_u32_le(&mut writer, 1)?;
    write_u32_le(&mut writer, width)?;
    write_u32_le(&mut writer, height)?;
    write_u32_le(&mut writer, cpp)?;
    write_u32_le(&mut writer, 16)?;
    write_u32_le(&mut writer, original_bits)?;

    for value in cfa_pattern(image) {
        write_i32_le(&mut writer, value)?;
    }
    for value in image.blacklevels {
        write_u16_le(&mut writer, value)?;
    }
    for value in image.whitelevels {
        write_u16_le(&mut writer, value)?;
    }

    write_u64_le(&mut writer, sample_count)?;
    for sample in samples {
        write_u16_le(&mut writer, sample)?;
    }
    writer.flush().map_err(|err| err.to_string())
}

fn run() -> Result<(), String> {
    let mut args = env::args_os();
    let _program = args.next();
    let input_path = args
        .next()
        .ok_or_else(|| "Usage: rawloader_bridge <input-path> <output-path>".to_string())?;
    let output_path = args
        .next()
        .ok_or_else(|| "Usage: rawloader_bridge <input-path> <output-path>".to_string())?;

    if args.next().is_some() {
        return Err("Usage: rawloader_bridge <input-path> <output-path>".to_string());
    }

    let image = rawloader::decode_file(Path::new(&input_path)).map_err(|err| err.to_string())?;
    write_payload(&image, Path::new(&output_path))
}

fn main() {
    if let Err(message) = run() {
        eprintln!("{message}");
        std::process::exit(1);
    }
}
