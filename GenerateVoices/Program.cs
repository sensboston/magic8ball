using System;
using System.Text;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using NAudio.Wave;
using NAudio.Lame;
using NAudio.Wave.SampleProviders;
using System.Reflection;

namespace GenerateVoices
{
    class Program
    {
        class Sentences
        {
            public string Language;
            public List<string> Content = new List<string>();
        }

        static Dictionary<string, string> LanguageCodes = new Dictionary<string, string>
        {
            { "English", "en-us" },
            { "Russian", "ru-ru" },
            { "Spanish", "es-mx" },
        };

        static List<Sentences> allSentences = new List<Sentences>();
        static string tempFileName = Path.Combine(Path.GetTempPath(), "tts.mp3");
        static Random rnd = new Random();

        /// <summary>
        /// This simple program generates "magic predictions" mp3s for "Digital Magic 8 Ball" project
        /// </summary>
        /// <param name="args"></param>
        static void Main(string[] args)
        {
            if (args.Length == 0 || !File.Exists(args[0]))
            {
                Console.WriteLine("Please provide source file full path for processing");
            }
            else
            {
                string content = File.ReadAllText(args[0]);
                int start = content.IndexOf("// 0: English");
                if (start > 2)
                {
                    int end = content.IndexOf("};", start);
                    if (end >= 0)
                    {
                        // Parse all phrases for all languages in source code
                        var lines = content.Substring(start, end - start).Replace("\t", "").Replace(", \"\", ", " ").Replace("{", "").Replace("\", \"", " ").Replace("}", "").Split(new[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
                        for (int i = 0; i < lines.Length; i++)
                        {
                            var s = lines[i].TrimStart(new[] { ' ' });
                            if (s.StartsWith("//"))
                            {
                                allSentences.Add(new Sentences());
                                allSentences.Last().Language = s.Substring(6).Trim();
                            }
                            else if (!string.IsNullOrEmpty(s) && s != ",")
                            {
                                int idx = s.IndexOf("//");
                                if (idx > 0) s = s.Substring(0, idx - 1);
                                allSentences.Last().Content.Add(s.Replace("\",", "").Replace("\"", "").Trim());
                            }
                        }

                        // Extract background sound(s)
                        var resNames = Assembly.GetExecutingAssembly().GetManifestResourceNames();
                        foreach (var name in resNames)
                        {
                            if (Path.GetExtension(name).Equals(".mp3"))
                            {
                                using (var resource = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
                                {
                                    string fileName = name.Substring(name.IndexOf(".") + 1);
                                    using (var file = new FileStream(fileName, FileMode.Create, FileAccess.Write)) resource.CopyTo(file);
                                }
                            }
                        }

                        // Process all languages
                        int fileNum = 1;
                        foreach (var sentence in allSentences)
                        {
                            var lang = LanguageCodes[sentence.Language];
                            Console.WriteLine($"Processing language: \"{sentence.Language}\" ");
                            double pitch = 0.3;
                            double speed = 0.3;
                            if (sentence.Language.StartsWith("Russian")) pitch = speed = 0.4;

                            foreach (var text in sentence.Content)
                            {
                                Console.WriteLine($"\tprocessing phrase: \"{text}\" ");
                                CloudTextToSpeech(tempFileName, text, lang, speed, pitch);
                                MixAudioFiles(tempFileName, $"magic{rnd.Next(1, 6)}.mp3", string.Format("{0:000}.mp3", fileNum++));
                            }
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Sorry, Google, but your official TTS API is too expensive and overcomplicated for homebrew projects!
        /// </summary>
        static void CloudTextToSpeech(string outFileName, string text, string lang, double speed = 0.4, double pitch = 0.4)
        {
            const string key = "AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw";
            using (WebClient client = new WebClient())
            {
                client.Headers.Add("user-agent", "Mozilla/4.0 (compatible; MSIE 6.0; Windows)");
                int txtLen = text.Length;
                text = "%" + BitConverter.ToString(Encoding.UTF8.GetBytes(text)).Replace("-", "%");
                string url = $"https://www.google.com/speech-api/v2/synthesize?enc=mpeg&client=chromium&key={key}&text={text}&lang={lang}&speed={speed}&pitch={pitch}";
                client.DownloadFile(url, outFileName);
            }
        }

        /// <summary>
        /// Mix speech and background sound 
        /// </summary>
        static void MixAudioFiles(string speechMp3, string backgroundMp3, string resultingMp3)
        {
            var backgroundSound = new AudioFileReader(backgroundMp3);
            var speechReader = new AudioFileReader(speechMp3);
            // We wanna shift speech for a few secodns
            var offsetProvider = new OffsetSampleProvider(speechReader);
            offsetProvider.DelayBy = TimeSpan.FromSeconds(3);
            var mixer = new MixingSampleProvider(new[] { offsetProvider });
            mixer.AddMixerInput((IWaveProvider)backgroundSound);

            var waveProvider = mixer.ToWaveProvider();
            using (var mp3Writer = new LameMP3FileWriter(resultingMp3, mixer.WaveFormat, 128))
            {
                var waveStream = new WaveProviderToWaveStream(waveProvider);
                waveStream.CopyTo(mp3Writer);
            }

            // Dispose used files
            backgroundSound.Dispose();
            backgroundSound.Close();
            speechReader.Dispose();
            speechReader.Close();
        }
    }

    public class WaveProviderToWaveStream : WaveStream
    {
        private readonly IWaveProvider source;
        private long position;

        public WaveProviderToWaveStream(IWaveProvider source)
        {
            this.source = source;
        }

        public override WaveFormat WaveFormat => source.WaveFormat;

        /// <summary>
        /// Don't know the real length of the source, just return a big number
        /// </summary>
        public override long Length => int.MaxValue; 

        public override long Position
        {
            get
            {
                // we'll just return the number of bytes read so far
                return position;
            }
            set
            {
                // can't set position on the source
                // n.b. could alternatively ignore this
                throw new NotImplementedException();
            }
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            int read = source.Read(buffer, offset, count);
            position += read;
            return read;
        }
    }
}
