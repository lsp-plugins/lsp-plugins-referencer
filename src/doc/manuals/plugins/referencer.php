<?php
	plugin_header();
	$m      =   ($PAGE == 'referencer_mono') ? 'm' : 's';
?>

<p>
	The refrencer plugin allows you to load your preferred reference files and compare them with your mix.
</p>
<p>
	It provides almost all the sound engineer needs to analyze the mix while performing mixing and mastering operations:
</p>
<ul>
<li>Loading of up to 4 audio files as reference tracks with playback of 4 independent loops per each file and simple
switch between files and loops using the <b>sample-loop matrix</b>.</li>
<li>Simple switch between the Mix sound and selected Reference sound. Possibility to mix the Mix and Reference signals together.</li>
<li>Automatic gain matching between Mix and Reference sounds.</li>
<?php if ($m == 's') { ?>
<li>Different monitoring modes of stereo signal.</li>
<?php } ?>
<li>Pre- or post-filtering of the signal for listening the specific band of the audio spectrum.</li>
<li>Measurement of such important parameters as: <b>Peak</b> and <b>True Peak</b> levels, <b>RMS</b>,
<b>Momentary</b>, <b>Short-Term</b> and <b>Integrated</b> LUFS.</li>
<li>Waveform analysis using linear or logarithmic scale.</li>
<li>Spectrum analysis<?php if ($m == 's') { ?>of <b>Left</b>, <b>Right</b>, <b>Mid</b>, <b>Side</b> parts of the stereo signal<?php } ?>.</li>
<li>Dynamics measurement - the measurement of the PSR (Peak-to-Short-Term Loudness Ratio) value as it is defined in the AES Convention 143 Brief 373 in and it's distribution.</li>
<?php if ($m == 's') { ?>
<li>Correlation and spectral correlation between left and right channels of the stereo track.</li>
<li>Goniometer for analyzing the stereo image of the track.</li>
<li>Stereo analysis that allows to analyze overall and spectral panorama between <b>Left</b> and <b>Right</b> channels.</li>
<li>Stereo analysis that allows to analyze the overall and spectral balance between the <b>Mid</b> and <b>Side</b> parts of the stereo signal.</li>
<?php } ?>
</ul>

<p><b>Source</b> section:</p>
<ul>
	<li><b>Mix</b> - the button that switches the referencer to play the input mix.</li>
	<li><b>Ref</b> - the button that switches the referencer to play the currently selected reference loop.</li>
	<li><b>Both</b> - the button that allows to mix both the input mix and the reference loop. When used, the mix and reference loop are attenuated by -3 dB.</li>
	<li><b>Play</b> - the play button that resumes the playback of currently selected loop.</li>
	<li><b>Stop</b> - the stop button that stops the playback of currently selected loop.</li>
	<li><b>Gain Matching</b> - the combo box that allows to set-up gain matcing:</li>
	<ul>
		<li><b>None</b> - the gain matching is not applied.</li>
		<li><b>Reference</b> - the gain of the reference signal is adjusted to match the loudness of the mix signal.</li>
		<li><b>Mix</b> - the gain of the mix signal is adjusted to match the loudness of the reference signal.</li>
	</ul>
	<li><b>Reactivity</b> - the speed of how quickly the gain is adjusted when matching the loudness.</li>
	<?php if ($m == 'm') { ?>
		<li><b>Audio sample graph</b> - the widget that allows to load currently selected audo file and monitor the playback of the currently selected loop.</li>
	<?php } ?>
</ul>

<?php if ($m == 's') { ?>
<p><b>Monitoring</b> section:</p>
<ul>
	<li><b>Stereo</b> - audio signal is played as a regular stereo.</li>
	<li><b>Reverse Stereo</b> - the left and right audio channels of the stereo output are swapped together.</li>
	<li><b>Mono</b> - both left and right outputs contain audio signal converted to mono (or Mid part of the signal).</li>
	<li><b>Side</b> - both left and right outputs contain the side part of the signal.</li>
	<li><b>Sides</b> - the left stereo output contains the side part of the output signal, the right stereo output
	 contains the phase-inverted side of the output signal.</li>
	<li><b>Mid/Side</b> - the left stereo output contains the mid part of signal, the right stereo output contains the side part of the signal.</li>
	<li><b>Side/Mid</b> - the left stereo output contains the side part of signal, the right stereo output contains the mid part of the signal.</li>
	<li><b>Left</b> - the left and right stereo outputs contain only the left channel of the signal.</li>
	<li><b>Right</b> - the left and right stereo outputs contain only the right channel of the signal.</li>
	<li><b>Left Only</b> - the right stereo output is muted.</li>
	<li><b>Right Only</b> - the left stereo output is muted.</li>
	<li><b>Audio sample graph</b> - the widget that allows to load currently selected audo file and monitor the playback of the currently selected loop.</li>
</ul>

<?php } ?>

<p><b>Sample-loop Matrix</b> section - the section that allows to select current audio file (sample) and loop to play. Each row is associated with
a file, and each button in a row is associated with the loop.</p>

<p><b>Analysis</b> section is responsible for tuning spectrum analysis and time analysis</p>
<ul>
	<li><b>Display</b> - allows to select for which audio signal graphs and charts will be drawn:</li>
	<ul>
		<li><b>Mix</b> - the button that allows drawing of charts for the mix signal.</li>
		<li><b>Ref</b> - the button that allows drawing of charts for the reference signal.</li>
	</ul>
	<li><b>Controls</b> - additional contol over the graphs and charts:</li>
	<ul>
		<li><b>Curr</b> - the button that turns on drawing of current values on spectrum-related graphs.</li>
		<li><b>Min</b> - the button that turns on drawing of minimums on spectrum-related graphs.</li>
		<li><b>Min</b> - the button that turns on drawing of maximums on spectrum-related graphs.</li>
		<li><b>Freeze</b> - the button that stops any update of graphs.</li>
		<li><b>Reset</b> - the button that resets minum and maximum values on spectrum-related graphs.</li>
	</ul>
	<li><b>Window</b> - the weighting window applied to the audio data before performing spectral analysis.</li>
	<li><b>Tolerance</b> - the number of points for the spectral analysis using FFT (Fast Fourier Transform).</li>
	<li><b>Envelope</b> - the additional envelope compensation of the signal on the spectrum-related graphs.</li>
	<li><b>Reactivity</b> - the reactivity (smoothness) of the spectral analysis.</li>
	<li><b>Damping</b> button - the button that enables damping of minimums and maximums.</li>
	<li><b>Damping</b> knob - the knob that controls the damping speed of minimums and maximums.</li>
	<li><b>Period</b> - the maximum time period displayed on the time graphs.</li>
</ul>

<p><b>Filter</b> section</p>
<ul>
	<li><b>Off</b> button - disables any filtering.</li>
	<li><b>Sub Bass</b> button - configures the filter to pass sub-bass band only (by default frequency range below 60 Hz).</li>
	<li><b>Bass</b> button - configures the filter to pass bass band only (by default frequency range between 60 Hz and 250 Hz).</li>
	<li><b>Bass</b> knob - configures the split frequency between sub-bass and bass bands.</li>
	<li><b>Low Mid</b> button - configures the filter to pass low-mid band only (by default frequency range between 250 Hz and 500 Hz).</li>
	<li><b>Low Mid</b> knob - configures the split frequency between bass and low-mid bands.</li>
	<li><b>Mid</b> button - configures the filter to pass mid band only (by default frequency range between 500 Hz and 2 kHz).</li>
	<li><b>Mid</b> knob - configures the split frequency between low-mid and mid bands.</li>
	<li><b>High Mid</b> button - configures the filter to pass high-mid band only (by default frequency range between 2 kHz and 6 kHz).</li>
	<li><b>High Mid</b> knob - configures the split frequency between mid and high-mid bands.</li>
	<li><b>High</b> button - configures the filter to pass high band only (by default frequency range above 6 kHz).</li>
	<li><b>High</b> knob - configures the split frequency between high-mid and high bands.</li>
	<li><b>Position</b> - the filter position:</li>
	<ul>
		<li><b>Pre-eq</b> - the filter is applied before any metering is performed.</li>
		<li><b>Post-Eq</b> - the filter is applied after any metering is performed.</li>
	</ul>
	<li><b>Steepness</b> - the combo box that allows to set-up the steepness of the filter.</li>
	<li><b>Mode</b> - filter processing mode:</li>
	<ul>
		<li><b>IIR</b> - Infinite Impulse Response filters, nonlinear minimal phase. In most cases does not add noticeable latency to output signal.</li>
		<li><b>FIR</b> - Finite Impulse Response filters with linear phase, finite approximation of equalizer's impulse response. Adds noticeable latency to output signal.</li>
		<li><b>FFT</b> - Fast Fourier Transform approximation of the frequency chart, linear phase. Adds noticeable latency to output signal.</li>
		<li><b>SPM</b> - Spectral Processor Mode of equalizer, equalizer transforms the magnitude of signal spectrum instead of applying impulse response to the signal.</li>
	</ul>
</ul>

