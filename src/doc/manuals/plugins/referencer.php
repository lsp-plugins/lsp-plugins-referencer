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

<p>All analysis tools are split into several tabs:</p>
<ul>
	<li><b>Overview</b> - the short overview of most useful analysis operations</li>
	<li><b>Samples</b> - the tab for loading audio references and configuring loops</li>
	<li><b>Loudness</b> - the loudness analysis</li>
	<li><b>Waveform</b> - the waveform analysis</li>
	<li><b>Spectrum</b> - the spectrum analysis</li>
	<li><b>Dynamics</b> - the analysis of dynamics of the signal</li>
<?php if ($m == 's') { ?>
	<li><b>Correlation</b> - the analysis of correlation</li>
	<li><b>Stereo</b> - different stereo analysis tools</li>
<?php } ?>
</ul>

<p>The <b>Overview</b> tab contains sections with elements from other tabs which are the following:</p>
<ul>
	<li><b>Spectrum</b> - the spectrum graph of mix and reference signals.</li>
	<li><b>Loudness</b> - the peak, RMS and LUFS meters. For more information see the description of '<b>Loudness</b>' tab.</li>
<?php if ($m == 's') { ?>
	<li><b>Correlation</b> - the common correlation and spectral correlation. For more information see the description of '<b>Correlation</b>' tab.</li>
<?php } ?>
	<li><b>Waveform</b> - the waveform of the mix and reference signals. For more information see the '<b>Waveform</b>' tab.</li>
	<li><b>Dynamics</b> - the dynamics analysis. For more information see the '<b>Dynamics</b>' tab.</li>
<?php if ($m == 's') { ?>
	<li><b>Goniometer</b> - the goniometer graph. For more information see the '<b>Stereo</b>' tab.</li>
<?php } ?>
</ul>
<p>The <b>Overivew</b> tab is useful for understanding the whole image about mix without significant details. Clicking on the corresponding
section allows to quickly switch to the desired tab.</p>

<p>The <b>Samples</b> tab contains graphs from other tabs which are the following:</p>
<ul>
	<li><b>Sample</b> - the sample combo group with the sample widget. Allows to select current sample and load it's contents from the file.</li>
	<li><b>Gain</b> - allows to adjust the loudness of the loaded audio sample.</li>
	<li><b>Loop 1</b> - <b>Loop 4</b> - buttons that allow to display the range of the selected loop.</li>
	<li><b>Loop 1 Start</b> - <b>Loop 4 Start</b> - the start position of the corresponding loop.</li>
	<li><b>Loop 1 End</b> - <b>Loop 4 End</b> - the end position of the corresponding loop.</li>
</ul>

<p>The <b>Loudness</b> tab allows to show time graph and meters for following values:</p>
<ul>
	<li><b>Peak (PK)</b> - the actual Peak value of the signal.</li>
	<li><b>True Peak (TP)</b> - the True Peak value of the signal as defined by BS.1770-5 specification.</li>
	<li><b>RMS (RMS)</b> - the RMS value of the signal.</li>
	<li><b>Momentary LUFS (M)</b> - the momentary LUFS value as defined by BS.1770-5 specification.</li>
	<li><b>Short-Term LUFS (S)</b> - the short-term LUFS value as defined by BS.1770-5 specification.</li>
	<li><b>Integrated LUFS (I)</b> - the integrated LUFS value (with gating) as defined by BS.1770-5 specification.</li>
</ul>
<p>The <b>Loudness</b> tabs contains following controls:</p>
<ul>
	<li><b>Peak</b> - the button that enables time graph with peak measurements.</li>
	<li><b>True Peak</b> - the button that enables time graph with true peak measurements.</li>
	<li><b>RMS</b> - the button that enables time graph with RMS measurements.</li>
	<li><b>Momentary LUFS</b> - the button that enables time graph with momentary LUFS measurements..</li>
	<li><b>Short-Term LUFS</b> - the button that enables time graph with short-term LUFS measurements.</li>
	<li><b>Integrated LUFS</b> - the button that enables time graph with integrated LUFS measurements.</li>
	<li><b>Integration</b> - the fader that allows to control the integration period for the integrated LUFS.</li>
</ul>

<p>The <b>Waveform</b> tab contains graphs with waveform analysis of the signal:</p>
<ul>
	<li><b>Zoom</b> - faders that allow to set up the upper threshold and lower threshold (if logarithmic scale is enabled).</li>
<?php if ($m == 's') { ?>
	<li><b>Left</b> - enables display of waveform for the left channel.</li>
	<li><b>Right</b> - enables display of waveform for the right channel.</li>
	<li><b>Mid</b> - enables display of waveform for the middle component of the signal.</li>
	<li><b>Side</b> - enables display of waveform for the side component of the signal.</li>
<?php } ?>
	<li><b>Log scale</b> - enables logarithmic scale for the signal.</li>
	<li><b>Frame</b> - sets the size of the time frame to display the waveform.</li>
	<li><b>Shift Mix</b> - additional time shift for the mix signal.</li>
	<li><b>Shift Reference</b> - additional time shift for the reference signal.</li>
</ul>
<p>The <b>zoom</b> also can be adjusted by clicking the grap with the right mouse button and moving mouse pointer up and down.</p>
<p>Alternative way to adjust mix and reference shifts can be achieved by clicking the graph with the left mouse button and
moving mouse pointer left and right. By default both Mix and Reference are moved. Holding the 'Shift' key allows to adjust the
shift of the mix only. Holding the 'Ctrl' key allows to adjust the shift of the reference only.</p>
<p>The size of the frame can be adjusted by applying mouse scroll on the graph. Holding the `Ctrl` key will accelerate the setup while
holding the 'Shift' key will decelerate the setup.</p>

<p>The <b>Spectrum</b> tab show the spectrum analysis of the selected signal:</p>
<ul>
<?php if ($m == 's') { ?>
	<li><b>Left</b> - enables display of waveform for the left channel.</li>
	<li><b>Right</b> - enables display of waveform for the right channel.</li>
	<li><b>Mid</b> - enables display of waveform for the middle component of the signal.</li>
	<li><b>Side</b> - enables display of waveform for the side component of the signal.</li>
<?php } ?>
	<li><b>Measure button</b> - enables additional horizontal line for measurements.</li>
	<li><b>Measure combo</b> - selects the source for which the vertical line on the graph will display frequency, note and current level.</li>
</ul>
<p>Moving the mouse over the graph shows the frequency, the note and the level on the graph the mouse pointer is pointing at current moment.</p>
<p>Clicking the graph will set the vertical measurement line to the corresponding position of the click.</p> 

<p>The <b>Dynamics</b> tab allows to analyze the micro-dynamics of the signal.</p>
<p>The advantage of the PSR value is that it does not depend on the loudness of the analyzed signal and allows to analyze the overall
compression applied to the Mix and Reference signals. The lower PRS value is, the less microdynamics has the track.</p>
<p>The following controls are available:</p>
<ul>
	<li><b>Time graph</b> - displays the change of the PSR value in the time.</li>
	<li><b>Meters</b> - display current PRS value for Mix and Reference signals.</li>
	<li><b>Distribution graph</b> - displays the density, frequency distribution or normalized frequency distribution of the PSR value across the time.</li>
	<li><b>Mode</b> - allows to select the graph to be displayed on the distribution graph:</li>
	<ul>
		<li><b>Density</b> - the graph displays the amount of time (in percents) the PSR value is higher than corresponding level.</li>
		<li><b>Frequency</b> - the graph displays the amount of time (in percents) the PSR value is equal to the corresponding level.</li>
		<li><b>Normalized</b> - same to the <b>Frequency</b> but values are normalized to consider the highest peak having the value 100% on the graph.</li>
	</ul>
	<li><b>Threshold</b> - the threshold that allows to control the specific point on the distribution graph to estimate the amount of time the PRS value is larger than specified threshold.</li>
	<li><b>Period</b> - the period of time used to compute chart for the distribution graph.</li>
</ul>

<?php if ($m == 's') { ?>
<p>The <b>Correlation</b> tab allows to analyze the overall correlation and frequency-domain correlation between left and right channels of the stereo track:</p>
<ul>
	<li><b>Mode</b> - allows to select the graph to be displayed:</li>
	<ul>
		<li><b>Spectrum</b> - displays the spectrum correlation graph.</li>
		<li><b>History</b> - displays the overall correlation changes in the time.</li>
	</ul>
</ul>

<p>The <b>Stereo</b> tab allows to perform some stereo analysis of the signal</p>:
<ul>
	<li><b>Mode</b> - the mode of the graph displayed on the left side of the tab:</li>
	<ul>
		<li><b>L/R Panorama</b> - displays the panorama position between the left and right channel.</li>
		<li><b>M/S Balance</b> - displays the balance between Mid and Side components of the signal.</li>
		<li><b>Spectrum</b> - displays the values across the whole frequency range.</li>
		<li><b>History</b> - displays how the value is changed in the time for the whole stereo signal.</li>
	</ul>
	<li><b>Goniometer</b> - the goniometer that allows to look at the stereo image of the track.</li>
	<li><b>Max History</b> - the maximum history (number of block) for drawing.</li>
	<li><b>Max Dots</b> - the maximum number of dots for drawing.</li>
</ul>
<?php } ?>
