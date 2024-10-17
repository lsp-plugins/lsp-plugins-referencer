<?php
	plugin_header();
	$m      =   ($PAGE == 'referencer_mono') ? 'm' : 's';
?>

<p>
	This is a template plugin (<?= ($m == 'm') ? 'mono' : 'stereo' ?> version);
</p>

<p><b>Controls:</b></p>
<ul>
	<li>
		<b>Bypass</b> - bypass switch, when turned on (led indicator is shining), the output signal is similar to input signal. That does not mean
		that the plugin is not working.
	</li>
</ul>
