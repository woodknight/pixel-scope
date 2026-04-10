use exif::{In, Reader, Tag};
use std::env;
use std::fs::File;
use std::io::{BufReader, BufWriter, Write};
use std::path::Path;

const MAGIC: &[u8; 8] = b"PSRDNG1\0";
const PAYLOAD_VERSION: u32 = 2;

#[derive(Debug, Clone)]
struct MetadataEntry {
    label: String,
    value: String,
}

fn write_u16_le(writer: &mut dyn Write, value: u16) -> Result<(), String> {
    writer
        .write_all(&value.to_le_bytes())
        .map_err(|err| err.to_string())
}

fn write_u32_le(writer: &mut dyn Write, value: u32) -> Result<(), String> {
    writer
        .write_all(&value.to_le_bytes())
        .map_err(|err| err.to_string())
}

fn write_u64_le(writer: &mut dyn Write, value: u64) -> Result<(), String> {
    writer
        .write_all(&value.to_le_bytes())
        .map_err(|err| err.to_string())
}

fn write_i32_le(writer: &mut dyn Write, value: i32) -> Result<(), String> {
    writer
        .write_all(&value.to_le_bytes())
        .map_err(|err| err.to_string())
}

fn write_f32_le(writer: &mut dyn Write, value: f32) -> Result<(), String> {
    writer
        .write_all(&value.to_le_bytes())
        .map_err(|err| err.to_string())
}

fn write_string(writer: &mut dyn Write, value: &str) -> Result<(), String> {
    let bytes = value.as_bytes();
    let len = u32::try_from(bytes.len()).map_err(|_| "String length exceeds u32 range.".to_string())?;
    write_u32_le(writer, len)?;
    writer.write_all(bytes).map_err(|err| err.to_string())
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
    let cfa = image.cropped_cfa();
    if image.cpp != 1 || !cfa.is_valid() || cfa.width != 2 || cfa.height != 2 {
        return [-1, -1, -1, -1];
    }

    [
        cfa.color_at(0, 0) as i32,
        cfa.color_at(0, 1) as i32,
        cfa.color_at(1, 0) as i32,
        cfa.color_at(1, 1) as i32,
    ]
}

fn integer_samples(image: &rawloader::RawImage) -> Result<Vec<u16>, String> {
    match &image.data {
        rawloader::RawImageData::Integer(samples) => Ok(samples.clone()),
        rawloader::RawImageData::Float(_) => Err(
            "rawloader returned a floating-point payload, which PixelScope does not support yet."
                .to_string(),
        ),
    }
}

fn exif_field_value(exif: &exif::Exif, tag: Tag) -> Option<String> {
    exif.get_field(tag, In::PRIMARY)
        .map(|field| field.display_value().with_unit(exif).to_string())
        .filter(|value| !value.trim().is_empty())
}

fn extract_exif_metadata(input_path: &Path) -> Vec<MetadataEntry> {
    let file = match File::open(input_path) {
        Ok(file) => file,
        Err(_) => return Vec::new(),
    };
    let mut reader = BufReader::new(file);
    let exif = match Reader::new().read_from_container(&mut reader) {
        Ok(exif) => exif,
        Err(_) => return Vec::new(),
    };

    let mut entries = Vec::new();
    let mut push_tag = |label: &str, tag: Tag| {
        if let Some(value) = exif_field_value(&exif, tag) {
            entries.push(MetadataEntry {
                label: label.to_string(),
                value,
            });
        }
    };

    push_tag("Exposure Time", Tag::ExposureTime);
    push_tag("ISO", Tag::PhotographicSensitivity);
    push_tag("ISO", Tag::ISOSpeed);
    push_tag("Aperture", Tag::FNumber);
    push_tag("Exposure Bias", Tag::ExposureBiasValue);
    push_tag("Focal Length", Tag::FocalLength);
    push_tag("Date", Tag::DateTimeOriginal);
    push_tag("Lens Model", Tag::LensModel);
    push_tag("Software", Tag::Software);
    push_tag("Artist", Tag::Artist);
    push_tag("Copyright", Tag::Copyright);

    entries
}

fn write_payload(image: &rawloader::RawImage, output_path: &Path, input_path: &Path) -> Result<(), String> {
    let samples = integer_samples(image)?;
    let width = u32::try_from(image.width).map_err(|_| "Image width exceeds u32 range.".to_string())?;
    let height = u32::try_from(image.height).map_err(|_| "Image height exceeds u32 range.".to_string())?;
    let cpp = u32::try_from(image.cpp).map_err(|_| "Channel count exceeds u32 range.".to_string())?;
    let original_bits = derive_original_bits(&samples, image.whitelevels);
    let sample_count =
        u64::try_from(samples.len()).map_err(|_| "Sample count exceeds u64 range.".to_string())?;
    let metadata_entries = extract_exif_metadata(input_path);
    let orientation = image.orientation.to_u16() as u32;

    let file = File::create(output_path).map_err(|err| err.to_string())?;
    let mut writer = BufWriter::new(file);
    writer.write_all(MAGIC).map_err(|err| err.to_string())?;
    write_u32_le(&mut writer, PAYLOAD_VERSION)?;
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

    write_u32_le(&mut writer, orientation)?;
    for value in image.wb_coeffs {
        write_f32_le(&mut writer, value)?;
    }
    for value in image.crops {
        write_u32_le(
            &mut writer,
            u32::try_from(value).map_err(|_| "Crop value exceeds u32 range.".to_string())?,
        )?;
    }
    for row in image.xyz_to_cam {
        for value in row {
            write_f32_le(&mut writer, value)?;
        }
    }
    write_string(&mut writer, &image.make)?;
    write_string(&mut writer, &image.model)?;
    write_string(&mut writer, &image.clean_make)?;
    write_string(&mut writer, &image.clean_model)?;
    write_u32_le(
        &mut writer,
        u32::try_from(metadata_entries.len()).map_err(|_| "Metadata entry count exceeds u32 range.".to_string())?,
    )?;
    for entry in metadata_entries {
        write_string(&mut writer, &entry.label)?;
        write_string(&mut writer, &entry.value)?;
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

    let input_path = input_path;
    let output_path = output_path;
    let image = rawloader::decode_file(Path::new(&input_path)).map_err(|err| err.to_string())?;
    write_payload(&image, Path::new(&output_path), Path::new(&input_path))
}

fn main() {
    if let Err(message) = run() {
        eprintln!("{message}");
        std::process::exit(1);
    }
}
